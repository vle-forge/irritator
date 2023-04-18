// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "irritator/format.hpp"
#include <filesystem>
#include <irritator/core.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <boost/ut.hpp>

#include <fmt/format.h>

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
    // #if defined(IRRITATOR_ENABLE_DEBUG)
    //     irt::is_fatal_breakpoint = false;
    // #endif

    using namespace boost::ut;

    "easy"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        mod.init(mod_init);
        pj.init(32);
        sim.init(256, 4096);

        auto& c1  = mod.alloc_simple_component();
        auto& s1  = mod.simple_components.get(c1.id.simple_id);
        auto& ch1 = mod.alloc(s1, irt::dynamics_type::counter);
        expect(irt::is_success(
          mod.connect_input(s1, 0, mod.children.get_id(ch1), 0)));

        auto& c2  = mod.alloc_simple_component();
        auto& s2  = mod.simple_components.get(c2.id.simple_id);
        auto& ch2 = mod.alloc(s2, irt::dynamics_type::time_func);
        expect(irt::is_success(
          mod.connect_output(s2, mod.children.get_id(ch2), 0, 0)));

        auto& c3   = mod.alloc_simple_component();
        auto& s3   = mod.simple_components.get(c3.id.simple_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
        expect(irt::is_success(mod.connect(
          s3, mod.children.get_id(ch31), 0, mod.children.get_id(ch32), 0)));

        expect(eq(mod.children.ssize(), 4));
        expect(eq(mod.connections.ssize(), 3));

        expect(irt::is_success(pj.set(mod, sim, c3)));
        expect(eq(pj.tree_nodes_size().first, 3));

        expect(eq(sim.models.ssize(), 2));
        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));

        if (m1->type == irt::dynamics_type::counter) {
            auto ret = sim.connect(*m2, 0, *m1, 0);
            expect(ret == irt::status::model_connect_already_exist);
        } else {
            auto ret = sim.connect(*m1, 0, *m2, 0);
            expect(ret == irt::status::model_connect_already_exist);
        }
    };

    "no-connection"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        mod.init(mod_init);
        pj.init(32);
        sim.init(256, 4096);

        auto& c1 = mod.alloc_simple_component();
        auto& s1 = mod.simple_components.get(c1.id.simple_id);
        mod.alloc(s1, irt::dynamics_type::counter);

        auto& c2 = mod.alloc_simple_component();
        auto& s2 = mod.simple_components.get(c2.id.simple_id);
        mod.alloc(s2, irt::dynamics_type::time_func);

        auto& c3 = mod.alloc_simple_component();
        auto& s3 = mod.simple_components.get(c3.id.simple_id);
        mod.alloc(s3, mod.components.get_id(c2));
        mod.alloc(s3, mod.components.get_id(c1));

        expect(eq(mod.children.ssize(), 4));
        expect(eq(mod.connections.ssize(), 0));

        expect(irt::is_success(pj.set(mod, sim, c3)));
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

        mod.init(mod_init);
        pj.init(32);
        sim.init(256, 4096);

        auto& c1  = mod.alloc_simple_component();
        auto& s1  = mod.simple_components.get(c1.id.simple_id);
        auto& ch1 = mod.alloc(s1, irt::dynamics_type::counter);
        expect(irt::is_success(
          mod.connect_input(s1, 0, mod.children.get_id(ch1), 0)));

        auto& c11  = mod.alloc_simple_component();
        auto& s11  = mod.simple_components.get(c11.id.simple_id);
        auto& ch11 = mod.alloc(s11, mod.components.get_id(c1));
        expect(irt::is_success(
          mod.connect_input(s11, 0, mod.children.get_id(ch11), 0)));

        auto& c2  = mod.alloc_simple_component();
        auto& s2  = mod.simple_components.get(c2.id.simple_id);
        auto& ch2 = mod.alloc(s2, irt::dynamics_type::time_func);
        expect(irt::is_success(
          mod.connect_output(s2, mod.children.get_id(ch2), 0, 0)));

        auto& c22  = mod.alloc_simple_component();
        auto& s22  = mod.simple_components.get(c22.id.simple_id);
        auto& ch22 = mod.alloc(s22, mod.components.get_id(c2));
        expect(irt::is_success(
          mod.connect_output(s22, mod.children.get_id(ch22), 0, 0)));

        auto& c3   = mod.alloc_simple_component();
        auto& s3   = mod.simple_components.get(c3.id.simple_id);
        auto& ch31 = mod.alloc(s3, mod.components.get_id(c22));
        auto& ch32 = mod.alloc(s3, mod.components.get_id(c11));
        expect(irt::is_success(mod.connect(
          s3, mod.children.get_id(ch31), 0, mod.children.get_id(ch32), 0)));

        expect(eq(mod.children.ssize(), 6));
        expect(eq(mod.connections.ssize(), 5));

        expect(irt::is_success(pj.set(mod, sim, c3)));
        expect(eq(pj.tree_nodes_size().first, 5));

        expect(eq(sim.models.ssize(), 2));

        auto* m1 = sim.models.try_to_get(0);
        expect(neq(m1, nullptr));
        auto* m2 = sim.models.try_to_get(1);
        expect(neq(m2, nullptr));

        if (m1->type == irt::dynamics_type::counter) {
            auto ret = sim.connect(*m2, 0, *m1, 0);
            expect(ret == irt::status::model_connect_already_exist);
        } else {
            auto ret = sim.connect(*m1, 0, *m2, 0);
            expect(ret == irt::status::model_connect_already_exist);
        }
    };

    "grid-3x3-empty-con"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        mod.init(mod_init);
        pj.init(mod_init.tree_capacity *= 10);
        sim.init(256, 4096);

        auto& c = mod.alloc_simple_component();
        auto& s = mod.simple_components.get(c.id.simple_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.row    = 5;
        g.column = 5;

        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 3; ++col)
                g.default_children[row][col] = mod.components.get_id(c);

        expect(irt::is_success(pj.set(mod, sim, cg)));
        expect(eq(pj.tree_nodes_size().first, g.row * g.column + 1));
        expect(eq(sim.models.ssize(), g.row * g.column));
    };

    "grid-3x3-empty-con-middle"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;
        irt::project              pj;
        irt::simulation           sim;

        mod.init(mod_init);
        pj.init(mod_init.tree_capacity *= 10);
        sim.init(256, 4096);

        auto& c = mod.alloc_simple_component();
        auto& s = mod.simple_components.get(c.id.simple_id);
        mod.alloc(s, irt::dynamics_type::counter);

        expect(eq(mod.children.ssize(), 1));
        expect(eq(mod.connections.ssize(), 0));

        auto& cg = mod.alloc_grid_component();
        auto& g  = mod.grid_components.get(cg.id.grid_id);
        g.row    = 5;
        g.column = 5;

        g.default_children[1][1] = mod.components.get_id(c);

        expect(irt::is_success(pj.set(mod, sim, cg)));
        expect(
          eq(pj.tree_nodes_size().first, (g.row - 2) * (g.column - 2) + 1));
        expect(eq(sim.models.ssize(), (g.row - 2) * (g.column - 2)));
    };

    "grid-3x3"_test = [] {
        irt::vector<char> buffer;
        irt::io_cache     cache;

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            mod.init(mod_init);
            pj.init(mod_init.tree_capacity *= 10);
            sim.init(256, 4096);

            auto& c1  = mod.alloc_simple_component();
            auto& s1  = mod.simple_components.get(c1.id.simple_id);
            auto& ch1 = mod.alloc(s1, irt::dynamics_type::counter);
            expect(irt::is_success(
              mod.connect_input(s1, 0, mod.children.get_id(ch1), 0)));

            auto& c2  = mod.alloc_simple_component();
            auto& s2  = mod.simple_components.get(c2.id.simple_id);
            auto& ch2 = mod.alloc(s2, irt::dynamics_type::time_func);
            expect(irt::is_success(
              mod.connect_output(s2, mod.children.get_id(ch2), 0, 0)));

            auto& c3   = mod.alloc_simple_component();
            auto& s3   = mod.simple_components.get(c3.id.simple_id);
            auto& ch31 = mod.alloc(s3, mod.components.get_id(c2));
            auto& ch32 = mod.alloc(s3, mod.components.get_id(c1));
            expect(irt::is_success(mod.connect(
              s3, mod.children.get_id(ch31), 0, mod.children.get_id(ch32), 0)));

            expect(eq(mod.children.ssize(), 4));
            expect(eq(mod.connections.ssize(), 3));

            auto& cg = mod.alloc_grid_component();
            auto& g  = mod.grid_components.get(cg.id.grid_id);
            g.row    = 5;
            g.column = 5;

            for (int row = 0; row < 3; ++row)
                for (int col = 0; col < 3; ++col)
                    g.default_children[row][col] = mod.components.get_id(c3);

            expect(irt::is_success(pj.set(mod, sim, cg)));
            expect(eq(pj.tree_nodes_size().first, g.row * g.column * 3 + 1));

            expect(eq(sim.models.ssize(), g.row * g.column * 2));

            expect(irt::is_success(irt::project_save(
              pj,
              mod,
              sim,
              cache,
              buffer,
              irt::json_pretty_print::indent_2_one_line_array)));
        }

        expect(buffer.size() > 0u);

        {
            irt::modeling_initializer mod_init;
            irt::modeling             mod;
            irt::project              pj;
            irt::simulation           sim;

            mod.init(mod_init);
            pj.init(mod_init.tree_capacity *= 10);
            sim.init(256, 4096);

            expect(irt::is_bad(irt::project_load(
              pj, mod, sim, cache, std::span(buffer.data(), buffer.size()))));
        }
    };

    "internal_component_io"_test = [] {
        irt::modeling_initializer mod_init;
        irt::modeling             mod;

        mod.init(mod_init);
        irt::component_id ids[irt::internal_component_count];

        mod.registred_paths.init(8);
        mod.dir_paths.init(32);
        mod.file_paths.init(256);

        expect(mod.components.can_alloc(irt::internal_component_count));

        auto& reg    = mod.alloc_registred();
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
            irt::format(file.path, "{}.irt", irt::internal_component_names[i]);

            auto& c    = mod.components.alloc();
            auto  c_id = mod.components.get_id(c);
            c.reg_path = reg_id;
            c.dir      = dir_id;
            c.file     = file_id;

            auto ret = mod.copy(irt::enum_cast<irt::internal_component>(i), c);
            ids[i]   = c_id;
            expect(is_success(ret));
        }

        for (int i = 0, e = irt::internal_component_count; i != e; ++i) {
            auto* c = mod.components.try_to_get(ids[i]);
            expect(c != nullptr);
            expect(irt::is_success(mod.save(*c)));
        }

        expect(irt::is_success(mod.fill_components(reg)));
        expect(mod.components.ssize() == irt::internal_component_count * 2);
    };
}
