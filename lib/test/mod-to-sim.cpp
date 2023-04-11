// Copyright (c) 2023 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <irritator/modeling.hpp>

#include <boost/ut.hpp>

int main()
{
#if defined(IRRITATOR_ENABLE_DEBUG)
    irt::is_fatal_breakpoint = false;
#endif

    using namespace boost::ut;

    "modeling-to-simulation-easy"_test = [] {
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

        expect(eq(mod.children.size(), 4));
        expect(eq(mod.connections.size(), 3));

        irt::modeling_to_simulation cache;

        expect(irt::is_success(irt::project_init(pj, mod, c3)));
        expect(eq(pj.tree_nodes.size(), 3));

        expect(irt::is_success(simulation_init(pj, mod, sim, cache)));

        expect(eq(sim.models.size(), 2));

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

    "modeling-to-simulation-empty-component"_test = [] {
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

        expect(eq(mod.children.size(), 6));
        expect(eq(mod.connections.size(), 5));

        irt::modeling_to_simulation cache;

        expect(irt::is_success(irt::project_init(pj, mod, c3)));
        expect(eq(pj.tree_nodes.size(), 5));

        expect(irt::is_success(simulation_init(pj, mod, sim, cache)));

        expect(eq(sim.models.size(), 2));

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
}