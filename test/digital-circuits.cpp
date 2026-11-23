// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Digital circuits built from the logical models and nested components: the
// expected results are exact, and the size of the circuits grows quickly.

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include <array>

#include <boost/ut.hpp>

namespace {

using irt::connection;
using irt::dynamics_type;

constexpr connection::port mdl(const int port) noexcept
{
    return connection::port{ .model = port };
}

constexpr connection::port compo(const irt::port_id port) noexcept
{
    return connection::port{ .compo = port };
}

struct xor_component {
    irt::component_id id;
    irt::port_id      a, b, y;
};

/** y = (a or b) and not (a and b). */
xor_component make_xor(irt::component_access& ids) noexcept
{
    xor_component ret;
    ret.id     = ids.alloc_generic_component();
    auto& c    = ids.components[ret.id];
    auto& gen  = ids.generic_components.get(c.id.generic_id);
    ret.a      = c.get_or_add_x("a");
    ret.b      = c.get_or_add_x("b");
    ret.y      = c.get_or_add_y("y");
    auto& lor  = gen.alloc(dynamics_type::logical_or_2);
    auto& land = gen.alloc(dynamics_type::logical_and_2);
    auto& inv  = gen.alloc(dynamics_type::logical_invert);
    auto& out  = gen.alloc(dynamics_type::logical_and_2);

    boost::ut::expect(!!gen.connect_input(ret.a, lor, mdl(0)));
    boost::ut::expect(!!gen.connect_input(ret.a, land, mdl(0)));
    boost::ut::expect(!!gen.connect_input(ret.b, lor, mdl(1)));
    boost::ut::expect(!!gen.connect_input(ret.b, land, mdl(1)));
    boost::ut::expect(!!gen.connect(land, mdl(0), inv, mdl(0)));
    boost::ut::expect(!!gen.connect(lor, mdl(0), out, mdl(0)));
    boost::ut::expect(!!gen.connect(inv, mdl(0), out, mdl(1)));
    boost::ut::expect(!!gen.connect_output(ret.y, out, mdl(0)));

    return ret;
}

struct full_adder_component {
    irt::component_id id;
    irt::port_id      a, b, cin, s, cout;
};

/** s = a xor b xor cin and cout = (a and b) or ((a xor b) and cin). */
full_adder_component make_full_adder(irt::component_access& ids,
                                     const xor_component&   x) noexcept
{
    full_adder_component ret;
    ret.id    = ids.alloc_generic_component();
    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.a     = c.get_or_add_x("a");
    ret.b     = c.get_or_add_x("b");
    ret.cin   = c.get_or_add_x("cin");
    ret.s     = c.get_or_add_y("s");
    ret.cout  = c.get_or_add_y("cout");
    auto& x1  = gen.alloc(x.id);
    auto& x2  = gen.alloc(x.id);
    auto& g1  = gen.alloc(dynamics_type::logical_and_2);
    auto& g2  = gen.alloc(dynamics_type::logical_and_2);
    auto& g3  = gen.alloc(dynamics_type::logical_or_2);

    boost::ut::expect(!!gen.connect_input(ret.a, x1, compo(x.a)));
    boost::ut::expect(!!gen.connect_input(ret.a, g1, mdl(0)));
    boost::ut::expect(!!gen.connect_input(ret.b, x1, compo(x.b)));
    boost::ut::expect(!!gen.connect_input(ret.b, g1, mdl(1)));
    boost::ut::expect(!!gen.connect_input(ret.cin, x2, compo(x.b)));
    boost::ut::expect(!!gen.connect_input(ret.cin, g2, mdl(1)));
    boost::ut::expect(!!gen.connect(x1, compo(x.y), x2, compo(x.a)));
    boost::ut::expect(!!gen.connect(x1, compo(x.y), g2, mdl(0)));
    boost::ut::expect(!!gen.connect(g1, mdl(0), g3, mdl(0)));
    boost::ut::expect(!!gen.connect(g2, mdl(0), g3, mdl(1)));
    boost::ut::expect(!!gen.connect_output(ret.s, x2, compo(x.y)));
    boost::ut::expect(!!gen.connect_output(ret.cout, g3, mdl(0)));

    return ret;
}

template<int Bits>
struct adder_component {
    irt::component_id              id;
    std::array<irt::port_id, Bits> a, b, s;
    irt::port_id                   cin, cout;
};

/** A ripple-carry adder: the carry of the bit i feeds the bit i + 1. */
template<int Bits>
adder_component<Bits> make_adder(irt::component_access&      ids,
                                 const full_adder_component& fa) noexcept
{
    adder_component<Bits> ret;
    ret.id    = ids.alloc_generic_component();
    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.cin   = c.get_or_add_x("cin");
    ret.cout  = c.get_or_add_y("cout");

    irt::generic_component::child* previous = nullptr;
    for (int i = 0; i < Bits; ++i) {
        ret.a[i] = c.get_or_add_x(irt::format_n<32>("a{}", i).sv());
        ret.b[i] = c.get_or_add_x(irt::format_n<32>("b{}", i).sv());
        ret.s[i] = c.get_or_add_y(irt::format_n<32>("s{}", i).sv());

        auto& bit = gen.alloc(fa.id);
        boost::ut::expect(!!gen.connect_input(ret.a[i], bit, compo(fa.a)));
        boost::ut::expect(!!gen.connect_input(ret.b[i], bit, compo(fa.b)));
        boost::ut::expect(!!gen.connect_output(ret.s[i], bit, compo(fa.s)));

        if (previous)
            boost::ut::expect(
              !!gen.connect(*previous, compo(fa.cout), bit, compo(fa.cin)));
        else
            boost::ut::expect(!!gen.connect_input(ret.cin, bit, compo(fa.cin)));

        previous = &bit;
    }

    boost::ut::expect(
      !!gen.connect_output(ret.cout, *previous, compo(fa.cout)));

    return ret;
}

/** The root: one constant per input bit and one counter per output bit, the
 * counter keeps the last value received. */
template<int Bits>
struct adder_bench {
    irt::component_id                   id;
    std::array<irt::child_id, 2 * Bits> inputs;
    std::array<irt::child_id, Bits + 1> outputs;
};

template<int Bits>
adder_bench<Bits> make_adder_bench(irt::component_access&       ids,
                                   const adder_component<Bits>& adder) noexcept
{
    adder_bench<Bits> ret;
    ret.id    = ids.alloc_generic_component();
    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    auto& add = gen.alloc(adder.id);

    for (int i = 0; i < Bits; ++i) {
        auto& ka = gen.alloc(dynamics_type::constant);
        auto& kb = gen.alloc(dynamics_type::constant);
        boost::ut::expect(!!gen.connect(ka, mdl(0), add, compo(adder.a[i])));
        boost::ut::expect(!!gen.connect(kb, mdl(0), add, compo(adder.b[i])));
        ret.inputs[i]        = gen.children.get_id(ka);
        ret.inputs[Bits + i] = gen.children.get_id(kb);
    }

    auto& kc = gen.alloc(dynamics_type::constant);
    boost::ut::expect(!!gen.connect(kc, mdl(0), add, compo(adder.cin)));

    for (int i = 0; i <= Bits; ++i) {
        auto& cnt = gen.alloc(dynamics_type::counter);
        boost::ut::expect(!!gen.connect(
          add, compo(i < Bits ? adder.s[i] : adder.cout), cnt, mdl(0)));
        ret.outputs[i] = gen.children.get_id(cnt);
    }

    return ret;
}

/** Builds the simulation of the bench with the inputs @c a and @c b and
 * returns the sum read on the output counters. */
template<int Bits>
int simulate_addition(irt::modeling&           mod,
                      irt::project&            pj,
                      const adder_bench<Bits>& bench,
                      const int                a,
                      const int                b) noexcept
{
    mod.ids.write([&](auto& ids) {
        auto& c   = ids.components[bench.id];
        auto& gen = ids.generic_components.get(c.id.generic_id);

        for (int i = 0; i < Bits; ++i) {
            gen.children_parameters[bench.inputs[i]].reals[0] = (a >> i) & 1;
            gen.children_parameters[bench.inputs[Bits + i]].reals[0] = (b >>
                                                                        i) &
                                                                       1;
        }
    });

    mod.ids.read([&](const auto& ids, auto) {
        mod.files.read([&](const auto& fs, auto) {
            boost::ut::expect(
              boost::ut::fatal(pj.set(ids, fs, bench.id).has_value()));
        });
    });

    pj.sim.limits.set_bound(0, 10);
    boost::ut::expect(boost::ut::fatal(pj.sim.initialize().has_value()));
    do {
        boost::ut::expect(boost::ut::fatal(pj.sim.run().has_value()));
    } while (not pj.sim.current_time_expired());

    const auto& head = *pj.tree_nodes.try_to_get(pj.tn_head());
    int         sum  = 0;
    for (int i = 0; i <= Bits; ++i) {
        const auto& out = head.children[bench.outputs[i]];
        const auto& cnt = irt::get_dyn<irt::counter>(
          pj.sim.models.get(out.mdl));
        if (cnt.last_value != 0.0)
            sum |= 1 << i;
    }

    return sum;
}

} // namespace

int main()
{
    using namespace boost::ut;

    "inverter-sends-its-initial-value"_test = [] {
        irt::simulation sim;
        auto&           inv = sim.alloc<irt::logical_invert>();
        auto&           cnt = sim.alloc<irt::counter>();
        expect(!!sim.connect_dynamics(inv, 0, cnt, 0));

        sim.limits.set_bound(0, 10);
        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
        } while (not sim.current_time_expired());

        expect(eq(cnt.event_number, irt::i64{ 1 }));
        expect(eq(cnt.last_value, 1.0));
    };

    "xor-follows-its-truth-table"_test = [] {
        for (int a = 0; a < 2; ++a) {
            for (int b = 0; b < 2; ++b) {
                irt::simulation sim;
                auto&           ka  = sim.alloc<irt::constant>();
                auto&           kb  = sim.alloc<irt::constant>();
                auto&           x   = sim.alloc<irt::logical_xor_2>();
                auto&           cnt = sim.alloc<irt::counter>();
                sim.parameters[sim.get_id(ka)].set_constant(a);
                sim.parameters[sim.get_id(kb)].set_constant(b);
                expect(!!sim.connect_dynamics(ka, 0, x, 0));
                expect(!!sim.connect_dynamics(kb, 0, x, 1));
                expect(!!sim.connect_dynamics(x, 0, cnt, 0));

                sim.limits.set_bound(0, 10);
                expect(fatal(sim.initialize().has_value()));
                do {
                    expect(fatal(sim.run().has_value()));
                } while (not sim.current_time_expired());

                expect(eq(cnt.last_value, a != b ? 1.0 : 0.0))
                  << a << " xor " << b;
            }
        }
    };

    "adder-4-bits-every-addition"_test = [] {
        irt::modeling mod;
        irt::project  pj;

        const auto bench = mod.ids.write([&](auto& ids) {
            const auto x     = make_xor(ids);
            const auto fa    = make_full_adder(ids, x);
            const auto adder = make_adder<4>(ids, fa);
            return make_adder_bench<4>(ids, adder);
        });

        for (int a = 0; a < 16; ++a)
            for (int b = 0; b < 16; ++b)
                expect(eq(simulate_addition<4>(mod, pj, bench, a, b), a + b))
                  << a << " + " << b;
    };
}
