// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Digital circuits built from the logical models and nested components: the
// expected results are exact, and the size of the circuits grows quickly.

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <utility>

#include <boost/ut.hpp>

#include "utils.hpp"

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

/** The input and output ports of a component, one per bit: a vector of
 * inputs or outputs is packed in an integer in the same order. */
struct circuit {
    irt::component_id         id;
    irt::vector<irt::port_id> x;
    irt::vector<irt::port_id> y;

    /** The clock input of a sequential circuit, fed by the bench. */
    std::optional<irt::port_id> clk = std::nullopt;
};

/** A signal of a generic component: an output port of a child or, without
 * child, an input port of the component. */
struct wire {
    std::optional<irt::child_id> child;
    connection::port             port;
};

/** Allocates the gates of a generic component and connects them. The children
 * are kept by identifier: the children container grows during the build. */
class netlist
{
public:
    explicit netlist(irt::generic_component& gen) noexcept
      : m_gen(gen)
    {}

    static wire input(const irt::port_id x) noexcept
    {
        return wire{ .child = std::nullopt, .port = compo(x) };
    }

    irt::child_id child(const irt::component_id id) noexcept
    {
        boost::ut::expect(boost::ut::fatal(m_gen.grow_children()));
        return m_gen.children.get_id(m_gen.alloc(id));
    }

    static wire output_of(const irt::child_id child,
                          const irt::port_id  y) noexcept
    {
        return wire{ .child = child, .port = compo(y) };
    }

    void link(const wire&            src,
              const irt::child_id    dst,
              const connection::port port) noexcept
    {
        auto& child_dst = m_gen.children.get(dst);

        boost::ut::expect(boost::ut::fatal(have_room(m_gen.connections) and
                                           have_room(m_gen.input_connections)));

        if (src.child)
            boost::ut::expect(!!m_gen.connect(m_gen.children.get(*src.child),
                                              src.port, child_dst, port));
        else
            boost::ut::expect(
              !!m_gen.connect_input(src.port.compo, child_dst, port));
    }

    void output(const irt::port_id y, const wire& src) noexcept
    {
        boost::ut::expect(boost::ut::fatal(src.child.has_value()));
        boost::ut::expect(
          boost::ut::fatal(have_room(m_gen.output_connections)));
        boost::ut::expect(
          !!m_gen.connect_output(y, m_gen.children.get(*src.child), src.port));
    }

    wire gate(const dynamics_type type, std::initializer_list<wire> inputs)
    {
        boost::ut::expect(boost::ut::fatal(m_gen.grow_children()));
        const auto id   = m_gen.children.get_id(m_gen.alloc(type));
        int        port = 0;
        for (const auto& in : inputs)
            link(in, id, mdl(port++));

        return wire{ .child = id, .port = mdl(0) };
    }

    wire not_(const wire& a)
    {
        return gate(dynamics_type::logical_invert, { a });
    }

    wire and_(const wire& a, const wire& b)
    {
        return gate(dynamics_type::logical_and_2, { a, b });
    }

    wire and_(const wire& a, const wire& b, const wire& c)
    {
        return gate(dynamics_type::logical_and_3, { a, b, c });
    }

    wire or_(const wire& a, const wire& b)
    {
        return gate(dynamics_type::logical_or_2, { a, b });
    }

    wire xor_(const wire& a, const wire& b)
    {
        return gate(dynamics_type::logical_xor_2, { a, b });
    }

    /** A tree of or gates: three inputs per gate as long as possible. */
    wire any(const std::span<const wire> inputs)
    {
        switch (inputs.size()) {
        case 1:
            return inputs[0];
        case 2:
            return or_(inputs[0], inputs[1]);
        case 3:
            return gate(dynamics_type::logical_or_3,
                        { inputs[0], inputs[1], inputs[2] });
        default: {
            const auto first  = (inputs.size() + 2) / 3;
            const auto second = (inputs.size() - first + 1) / 2;
            return gate(dynamics_type::logical_or_3,
                        { any(inputs.subspan(0, first)),
                          any(inputs.subspan(first, second)),
                          any(inputs.subspan(first + second)) });
        }
        }
    }

    /** Places each child in the column of its longest path from the inputs
     * of the component, to read the circuit from left to right in the
     * editor. */
    void place() noexcept
    {
        const auto       capacity = m_gen.children.capacity();
        irt::vector<int> depth(capacity, 0);

        // The flip-flops start the columns: the connections into them close
        // the loops of a sequential circuit. The passes are bounded by the
        // longest path without loop.
        const auto is_flipflop = [&](const irt::child_id id) {
            const auto& child = m_gen.children.get(id);
            return child.type == irt::child_type::model and
                   child.id.mdl_type == dynamics_type::logical_d_flipflop;
        };

        auto changed = true;
        for (int pass = 0, e = m_gen.children.ssize(); changed and pass < e;
             ++pass) {
            changed = false;
            for (const auto& con : m_gen.connections) {
                if (is_flipflop(con.dst))
                    continue;

                const auto src = irt::get_index(con.src);
                const auto dst = irt::get_index(con.dst);
                if (depth[dst] <= depth[src]) {
                    depth[dst] = depth[src] + 1;
                    changed    = true;
                }
            }
        }

        // A loop through a child component is not broken: its depth may
        // exceed the number of children.
        irt::vector<int> rows(*std::ranges::max_element(depth) + 1, 0);
        for (const auto& child : m_gen.children) {
            const auto id    = m_gen.children.get_id(child);
            const auto level = depth[irt::get_index(id)];

            m_gen.children_positions[id] = irt::position{
                .x = static_cast<float>(level) * 200.f,
                .y = static_cast<float>(rows[level]++) * 80.f
            };
        }
    }

private:
    template<typename Connections>
    static bool have_room(Connections& connections) noexcept
    {
        return connections.can_alloc(1) or connections.template grow<2, 1>();
    }

    irt::generic_component& m_gen;
};

struct full_adder_component {
    irt::component_id id;
    irt::port_id      a, b, cin, s, cout;
};

/** s = a xor b xor cin and cout = (a and b) or ((a xor b) and cin). */
full_adder_component make_full_adder(irt::component_access& ids) noexcept
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

    netlist    n(gen);
    const auto a   = netlist::input(ret.a);
    const auto b   = netlist::input(ret.b);
    const auto cin = netlist::input(ret.cin);
    const auto ab  = n.xor_(a, b);

    n.output(ret.s, n.xor_(ab, cin));
    n.output(ret.cout, n.or_(n.and_(a, b), n.and_(ab, cin)));
    n.place();

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

    netlist n(gen);
    wire    carry = netlist::input(ret.cin);
    for (int i = 0; i < Bits; ++i) {
        ret.a[i] = c.get_or_add_x(irt::format_n<32>("a{}", i).sv());
        ret.b[i] = c.get_or_add_x(irt::format_n<32>("b{}", i).sv());
        ret.s[i] = c.get_or_add_y(irt::format_n<32>("s{}", i).sv());

        const auto bit = n.child(fa.id);
        n.link(netlist::input(ret.a[i]), bit, compo(fa.a));
        n.link(netlist::input(ret.b[i]), bit, compo(fa.b));
        n.link(carry, bit, compo(fa.cin));
        n.output(ret.s[i], netlist::output_of(bit, fa.s));
        carry = netlist::output_of(bit, fa.cout);
    }

    n.output(ret.cout, carry);
    n.place();

    return ret;
}

/** Inputs: a, b then cin. Outputs: s then cout. */
template<int Bits>
circuit as_circuit(const adder_component<Bits>& adder) noexcept
{
    circuit ret{ .id = adder.id,
                 .x = irt::vector<irt::port_id>(2 * Bits + 1, irt::reserve_tag),
                 .y = irt::vector<irt::port_id>(Bits + 1, irt::reserve_tag) };

    for (const auto port : adder.a)
        ret.x.emplace_back(port);
    for (const auto port : adder.b)
        ret.x.emplace_back(port);
    ret.x.emplace_back(adder.cin);

    for (const auto port : adder.s)
        ret.y.emplace_back(port);
    ret.y.emplace_back(adder.cout);

    return ret;
}

enum class alu_op : unsigned { ora, and_, eor, adc, sbc, rol, ror, pass_b };

constexpr unsigned alu_op_count = 8;

/** The results of the 6502 operations on a, b and the carry: SBC is an ADC
 * of the complement of b, and the carry of the logical operations is kept. */
struct alu_result {
    unsigned r, c, v;
};

constexpr alu_result alu_reference(const alu_op   op,
                                   const unsigned a,
                                   const unsigned b,
                                   const unsigned cin) noexcept
{
    const auto add = [](const unsigned x, const unsigned y, const unsigned c) {
        const auto sum = x + y + c;
        const auto r   = sum & 0xffu;
        const auto v   = ((x ^ r) & (y ^ r) & 0x80u) != 0 ? 1u : 0u;
        return alu_result{ r, sum >> 8, v };
    };

    switch (op) {
    case alu_op::ora:
        return { a | b, cin, 0 };
    case alu_op::and_:
        return { a & b, cin, 0 };
    case alu_op::eor:
        return { a ^ b, cin, 0 };
    case alu_op::adc:
        return add(a, b, cin);
    case alu_op::sbc:
        return add(a, ~b & 0xffu, cin);
    case alu_op::rol:
        return { ((a << 1) | cin) & 0xffu, a >> 7, 0 };
    case alu_op::ror:
        return { (a >> 1) | (cin << 7), a & 1u, 0 };
    case alu_op::pass_b:
        return { b, cin, 0 };
    }

    return {};
}

static_assert(alu_reference(alu_op::adc, 0x50, 0x50, 0).v == 1);
static_assert(alu_reference(alu_op::sbc, 0x50, 0x10, 1).r == 0x40);
static_assert(alu_reference(alu_op::sbc, 0x50, 0x10, 1).c == 1);

/** Inputs: a0..a7, b0..b7, cin then op0..op2. Outputs: r0..r7, c, v, n and
 * z. */
constexpr irt::u64 alu_inputs(const alu_op   op,
                              const unsigned a,
                              const unsigned b,
                              const unsigned cin) noexcept
{
    return a | (b << 8) | (cin << 16) | (static_cast<irt::u64>(op) << 17);
}

constexpr irt::u64 alu_outputs(const alu_result& res) noexcept
{
    const auto n = res.r >> 7;
    const auto z = res.r == 0 ? 1u : 0u;
    return res.r | (res.c << 8) | (res.v << 9) | (n << 10) | (z << 11);
}

/** An 8 bits ALU: a decoder of the operation selects, for each bit of the
 * result, one of the candidates computed in parallel. */
circuit make_alu(irt::component_access&    ids,
                 const adder_component<8>& adder) noexcept
{
    circuit ret{ .id = ids.alloc_generic_component(),
                 .x  = irt::vector<irt::port_id>(20, irt::reserve_tag),
                 .y  = irt::vector<irt::port_id>(12, irt::reserve_tag) };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    std::array<wire, 8> a, b;
    for (int i = 0; i < 8; ++i)
        a[i] = netlist::input(
          *ret.x.emplace_back(c.get_or_add_x(irt::format_n<8>("a{}", i).sv())));
    for (int i = 0; i < 8; ++i)
        b[i] = netlist::input(
          *ret.x.emplace_back(c.get_or_add_x(irt::format_n<8>("b{}", i).sv())));
    const auto cin = netlist::input(*ret.x.emplace_back(c.get_or_add_x("cin")));

    std::array<wire, 3> op;
    for (int i = 0; i < 3; ++i)
        op[i] = netlist::input(*ret.x.emplace_back(
          c.get_or_add_x(irt::format_n<8>("op{}", i).sv())));

    netlist n(gen);

    const std::array<wire, 3> not_op{ n.not_(op[0]), n.not_(op[1]),
                                      n.not_(op[2]) };

    std::array<wire, alu_op_count> select;
    for (unsigned k = 0; k < alu_op_count; ++k)
        select[k] = n.and_(k & 1u ? op[0] : not_op[0],
                           k & 2u ? op[1] : not_op[1],
                           k & 4u ? op[2] : not_op[2]);

    const auto is = [&](const alu_op o) { return select[static_cast<int>(o)]; };

    const auto sub   = is(alu_op::sbc);
    const auto arith = n.or_(is(alu_op::adc), sub);

    const auto          add = n.child(adder.id);
    std::array<wire, 8> b_add, s;
    for (int i = 0; i < 8; ++i) {
        b_add[i] = n.xor_(b[i], sub);
        n.link(a[i], add, compo(adder.a[i]));
        n.link(b_add[i], add, compo(adder.b[i]));
        s[i] = netlist::output_of(add, adder.s[i]);
    }
    n.link(cin, add, compo(adder.cin));

    std::array<wire, 8> r;
    for (int i = 0; i < 8; ++i) {
        const std::array candidates{
            n.and_(is(alu_op::ora), n.or_(a[i], b[i])),
            n.and_(is(alu_op::and_), n.and_(a[i], b[i])),
            n.and_(is(alu_op::eor), n.xor_(a[i], b[i])),
            n.and_(arith, s[i]),
            n.and_(is(alu_op::rol), i == 0 ? cin : a[i - 1]),
            n.and_(is(alu_op::ror), i == 7 ? cin : a[i + 1]),
            n.and_(is(alu_op::pass_b), b[i]),
        };

        r[i] = n.any(candidates);
    }

    const std::array keep_carry{ is(alu_op::ora), is(alu_op::and_),
                                 is(alu_op::eor), is(alu_op::pass_b) };
    const std::array carry{
        n.and_(n.any(keep_carry), cin),
        n.and_(arith, netlist::output_of(add, adder.cout)),
        n.and_(is(alu_op::rol), a[7]),
        n.and_(is(alu_op::ror), a[0]),
    };

    const auto overflow = n.and_(
      arith, n.and_(n.xor_(a[7], s[7]), n.xor_(b_add[7], s[7])));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("r{}", i).sv())),
          r[i]);

    n.output(*ret.y.emplace_back(c.get_or_add_y("c")), n.any(carry));
    n.output(*ret.y.emplace_back(c.get_or_add_y("v")), overflow);
    n.output(*ret.y.emplace_back(c.get_or_add_y("n")), r[7]);
    n.output(*ret.y.emplace_back(c.get_or_add_y("z")), n.not_(n.any(r)));
    n.place();

    return ret;
}

/** An incrementer of @c Bits bits: a chain of half adders, the carry of the
 * bit 0 is one. */
template<int Bits>
std::array<wire, Bits> make_incrementer(netlist&                      n,
                                        const std::array<wire, Bits>& q)
{
    std::array<wire, Bits> ret;
    ret[0]     = n.not_(q[0]);
    auto carry = q[0];

    for (int i = 1; i < Bits; ++i) {
        ret[i] = n.xor_(q[i], carry);
        if (i + 1 < Bits)
            carry = n.and_(q[i], carry);
    }

    return ret;
}

/** A counter of @c Bits bits: a register of D flip-flops fed by the
 * incrementer of its own outputs, one increment per rising edge. */
template<int Bits>
circuit make_counter(irt::component_access& ids) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(0, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(Bits, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.clk   = c.get_or_add_x("clk");

    netlist    n(gen);
    const auto clk = netlist::input(*ret.clk);

    std::array<wire, Bits> q;
    for (int i = 0; i < Bits; ++i)
        q[i] = n.gate(dynamics_type::logical_d_flipflop, {});

    const auto next = make_incrementer<Bits>(n, q);
    for (int i = 0; i < Bits; ++i) {
        n.link(next[i], *q[i].child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *q[i].child, mdl(irt::logical_d_flipflop::port_clk));
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("q{}", i).sv())),
          q[i]);
    }

    n.place();

    return ret;
}

/** A D flip-flop which takes d on a rising edge of clk only if load is true:
 * a multiplexer between d and its own output. @c hold is not load, shared by
 * the flip-flops loaded together. */
wire make_loadable_flipflop(netlist&    n,
                            const wire& d,
                            const wire& load,
                            const wire& hold,
                            const wire& clk)
{
    const auto q    = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto next = n.or_(n.and_(load, d), n.and_(hold, q));

    n.link(next, *q.child, mdl(irt::logical_d_flipflop::port_d));
    n.link(clk, *q.child, mdl(irt::logical_d_flipflop::port_clk));

    return q;
}

/** A register of @c Bits bits: on a rising edge of clk, q takes d if load is
 * true and keeps its value otherwise. Inputs: d0.. then load. Outputs: q0..
 */
template<int Bits>
circuit make_register(irt::component_access& ids) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(Bits + 1, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(Bits, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    std::array<wire, Bits> d;
    for (int i = 0; i < Bits; ++i)
        d[i] = netlist::input(
          *ret.x.emplace_back(c.get_or_add_x(irt::format_n<8>("d{}", i).sv())));
    const auto load = netlist::input(
      *ret.x.emplace_back(c.get_or_add_x("load")));
    ret.clk        = c.get_or_add_x("clk");
    const auto clk = netlist::input(*ret.clk);

    netlist    n(gen);
    const auto hold = n.not_(load);

    for (int i = 0; i < Bits; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("q{}", i).sv())),
          make_loadable_flipflop(n, d[i], load, hold, clk));

    n.place();

    return ret;
}

/** The accumulator of the 6502: the ALU computes from A and the inputs, and
 * its result is loaded in A on a rising edge when load is true. Inputs: b0..b7,
 * cin, op0..op2 then load. Outputs: a0..a7 then the c, v, n and z flags of
 * the ALU. */
circuit make_accumulator(irt::component_access& ids,
                         const circuit&         alu,
                         const circuit&         reg) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(13, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(12, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    netlist    n(gen);
    const auto u = n.child(alu.id);
    const auto a = n.child(reg.id);

    // The ALU inputs: a0..a7, b0..b7, cin then op0..op2.
    for (int i = 0; i < 8; ++i)
        n.link(netlist::output_of(a, reg.y[i]), u, compo(alu.x[i]));

    for (int i = 8; i < 20; ++i) {
        const auto name = i < 16    ? irt::format_n<8>("b{}", i - 8)
                          : i == 16 ? irt::format_n<8>("cin")
                                    : irt::format_n<8>("op{}", i - 17);
        n.link(netlist::input(*ret.x.emplace_back(c.get_or_add_x(name.sv()))),
               u, compo(alu.x[i]));
    }

    // The register inputs: d0..d7 then load.
    for (int i = 0; i < 8; ++i)
        n.link(netlist::output_of(u, alu.y[i]), a, compo(reg.x[i]));
    n.link(netlist::input(*ret.x.emplace_back(c.get_or_add_x("load"))), a,
           compo(reg.x[8]));

    ret.clk = c.get_or_add_x("clk");
    n.link(netlist::input(*ret.clk), a, compo(*reg.clk));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("a{}", i).sv())),
          netlist::output_of(a, reg.y[i]));

    constexpr std::array<const char*, 4> flags{ "c", "v", "n", "z" };
    for (int i = 0; i < 4; ++i)
        n.output(*ret.y.emplace_back(c.get_or_add_y(flags[i])),
                 netlist::output_of(u, alu.y[8 + i]));

    n.place();

    return ret;
}

/** The C, V, N and Z flags of the status register P: C and V are loaded
 * apart, N and Z together, as the 6502 instructions do. Inputs: c, v, n, z,
 * load_c, load_v then load_nz. Outputs: c, v, n then z. */
circuit make_flags(irt::component_access& ids) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(7, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(4, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    constexpr std::array<const char*, 4> names{ "c", "v", "n", "z" };
    std::array<wire, 4>                  d;
    for (int i = 0; i < 4; ++i)
        d[i] = netlist::input(*ret.x.emplace_back(c.get_or_add_x(names[i])));

    constexpr std::array<const char*, 3> load_names{ "load_c", "load_v",
                                                     "load_nz" };
    std::array<wire, 3>                  load;
    for (int i = 0; i < 3; ++i)
        load[i] = netlist::input(
          *ret.x.emplace_back(c.get_or_add_x(load_names[i])));

    ret.clk        = c.get_or_add_x("clk");
    const auto clk = netlist::input(*ret.clk);

    netlist                   n(gen);
    const std::array<wire, 3> hold{ n.not_(load[0]), n.not_(load[1]),
                                    n.not_(load[2]) };

    // The flags c, v, n and z use the loads 0, 1, 2 and 2.
    constexpr std::array<int, 4> load_of{ 0, 1, 2, 2 };
    for (int i = 0; i < 4; ++i) {
        const auto l = load_of[i];
        n.output(*ret.y.emplace_back(c.get_or_add_y(names[i])),
                 make_loadable_flipflop(n, d[i], load[l], hold[l], clk));
    }

    n.place();

    return ret;
}

/** The execution unit: the accumulator and the flags, the carry of the ALU
 * is the stored C flag as for ADC, SBC, ROL and ROR. Inputs: b0..b7,
 * op0..op2, load_a, load_c, load_v then load_nz. Outputs: a0..a7 then the
 * stored c, v, n and z flags. */
circuit make_execution_unit(irt::component_access& ids,
                            const circuit&         acc,
                            const circuit&         flags) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(15, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(12, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    netlist    n(gen);
    const auto a = n.child(acc.id);
    const auto p = n.child(flags.id);

    const auto input = [&](const std::string_view name) {
        return netlist::input(*ret.x.emplace_back(c.get_or_add_x(name)));
    };

    // The accumulator inputs: b0..b7, cin, op0..op2 then load.
    for (int i = 0; i < 8; ++i)
        n.link(input(irt::format_n<8>("b{}", i).sv()), a, compo(acc.x[i]));
    n.link(netlist::output_of(p, flags.y[0]), a, compo(acc.x[8]));
    for (int i = 0; i < 3; ++i)
        n.link(input(irt::format_n<8>("op{}", i).sv()), a, compo(acc.x[9 + i]));
    n.link(input("load_a"), a, compo(acc.x[12]));

    // The flags inputs: the c, v, n and z of the ALU then the three loads.
    for (int i = 0; i < 4; ++i)
        n.link(netlist::output_of(a, acc.y[8 + i]), p, compo(flags.x[i]));
    n.link(input("load_c"), p, compo(flags.x[4]));
    n.link(input("load_v"), p, compo(flags.x[5]));
    n.link(input("load_nz"), p, compo(flags.x[6]));

    ret.clk        = c.get_or_add_x("clk");
    const auto clk = netlist::input(*ret.clk);
    n.link(clk, a, compo(*acc.clk));
    n.link(clk, p, compo(*flags.clk));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("a{}", i).sv())),
          netlist::output_of(a, acc.y[i]));

    constexpr std::array<const char*, 4> names{ "c", "v", "n", "z" };
    for (int i = 0; i < 4; ++i)
        n.output(*ret.y.emplace_back(c.get_or_add_y(names[i])),
                 netlist::output_of(p, flags.y[i]));

    n.place();

    return ret;
}

/** The program counter: on a rising edge, pc takes d if load is true (a jump),
 * pc + 1 if inc is true (the next byte), and keeps its value otherwise.
 * Inputs: d0..d15, load then inc. Outputs: pc0..pc15. */
circuit make_program_counter(irt::component_access& ids) noexcept
{
    constexpr int bits = 16;

    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(bits + 2, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(bits, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    std::array<wire, bits> d;
    for (int i = 0; i < bits; ++i)
        d[i] = netlist::input(
          *ret.x.emplace_back(c.get_or_add_x(irt::format_n<8>("d{}", i).sv())));
    const auto load = netlist::input(
      *ret.x.emplace_back(c.get_or_add_x("load")));
    const auto inc = netlist::input(*ret.x.emplace_back(c.get_or_add_x("inc")));
    ret.clk        = c.get_or_add_x("clk");
    const auto clk = netlist::input(*ret.clk);

    netlist n(gen);

    std::array<wire, bits> pc;
    for (int i = 0; i < bits; ++i)
        pc[i] = n.gate(dynamics_type::logical_d_flipflop, {});

    const auto next_pc   = make_incrementer<bits>(n, pc);
    const auto no_load   = n.not_(load);
    const auto increment = n.and_(no_load, inc);
    const auto keep      = n.and_(no_load, n.not_(inc));

    for (int i = 0; i < bits; ++i) {
        const std::array candidates{ n.and_(load, d[i]),
                                     n.and_(increment, next_pc[i]),
                                     n.and_(keep, pc[i]) };

        n.link(n.any(candidates), *pc[i].child,
               mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *pc[i].child, mdl(irt::logical_d_flipflop::port_clk));
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("pc{}", i).sv())),
          pc[i]);
    }

    n.place();

    return ret;
}

/** A component around a logical_memory of 64 KiB, the first bytes are the @a
 * content read from a constant source of the component at the initialization.
 * Inputs: a0..a15, d0..d7 then we. Outputs: q0..q7. */
circuit make_memory(irt::component_access&   ids,
                    std::span<const irt::u8> content = {}) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(25, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(8, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    netlist    n(gen);
    const auto mem = n.gate(dynamics_type::logical_memory, {});

    if (not content.empty()) {
        boost::ut::expect(
          boost::ut::fatal(c.srcs.data.can_alloc(1) or c.srcs.data.reserve(1)));
        const auto src_id = c.srcs.data.alloc_id();
        c.srcs.emplace(src_id, irt::source_type::constant, "content");

        auto& cst = c.srcs.data
                      .get<irt::external_source_definition::source_element>(
                        src_id)
                      .cst;
        boost::ut::expect(boost::ut::fatal(cst.data.resize(content.size())));
        std::copy(content.begin(), content.end(), cst.data.begin());

        gen.children_parameters[*mem.child].set_logical_memory(
          src_id, static_cast<irt::i32>(content.size()));
    }

    using irt::logical_memory;
    const auto input = [&](const std::string_view name, const int port) {
        const auto x = *ret.x.emplace_back(c.get_or_add_x(name));
        n.link(netlist::input(x), *mem.child, mdl(port));
    };

    for (int i = 0; i < 16; ++i)
        input(irt::format_n<8>("a{}", i).sv(), logical_memory::port_a + i);
    for (int i = 0; i < 8; ++i)
        input(irt::format_n<8>("d{}", i).sv(), logical_memory::port_d + i);
    input("we", logical_memory::port_we);

    ret.clk = c.get_or_add_x("clk");
    n.link(netlist::input(*ret.clk), *mem.child, mdl(logical_memory::port_clk));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("q{}", i).sv())),
          wire{ .child = mem.child, .port = mdl(i) });

    n.place();

    return ret;
}

/** The opcodes of the 6502 run by the CPU. */
enum class opcode : irt::u8 {
    ora_imm = 0x09,
    and_imm = 0x29,
    eor_imm = 0x49,
    adc_imm = 0x69,
    lda_imm = 0xa9,
    sbc_imm = 0xe9,
    sta_abs = 0x8d,
    jmp_abs = 0x4c,
};

constexpr std::array immediate_opcodes{ opcode::ora_imm, opcode::and_imm,
                                        opcode::eor_imm, opcode::adc_imm,
                                        opcode::lda_imm, opcode::sbc_imm };

/** The registers of the 6502 seen from the outputs of the CPU. */
struct cpu_state {
    unsigned pc = 0, a = 0, c = 0, v = 0, n = 0, z = 0;
};

/** Executes the instruction at pc, as the 6502 does, and returns its number
 * of cycles. */
constexpr int cpu_reference(cpu_state& s, std::span<irt::u8> mem) noexcept
{
    const auto byte = [&](const unsigned offset) -> unsigned {
        return mem[(s.pc + offset) & 0xffffu];
    };
    const auto address = byte(1) | (byte(2) << 8);
    const auto op      = static_cast<opcode>(byte(0));

    if (op == opcode::sta_abs) {
        mem[address] = static_cast<irt::u8>(s.a);
        s.pc         = (s.pc + 3) & 0xffffu;
        return 4;
    }

    if (op == opcode::jmp_abs) {
        s.pc = address;
        return 3;
    }

    // The bits 7..5 of the immediate opcodes select the operation.
    constexpr std::array<alu_op, 8> ops{ alu_op::ora,    alu_op::and_,
                                         alu_op::eor,    alu_op::adc,
                                         alu_op::pass_b, alu_op::pass_b,
                                         alu_op::pass_b, alu_op::sbc };
    const auto                      alu = ops[static_cast<unsigned>(op) >> 5];
    const auto                      res = alu_reference(alu, s.a, byte(1), s.c);
    s.a                                 = res.r;
    s.n                                 = res.r >> 7;
    s.z                                 = res.r == 0 ? 1u : 0u;
    if (alu == alu_op::adc or alu == alu_op::sbc) {
        s.c = res.c;
        s.v = res.v;
    }

    s.pc = (s.pc + 2) & 0xffffu;
    return 2;
}

/** The outputs of the CPU: pc0..pc15, a0..a7, c, v, n, z then sync. */
constexpr irt::u64 cpu_outputs(const cpu_state& s, const unsigned sync) noexcept
{
    return s.pc | (irt::u64{ s.a } << 16) | (irt::u64{ s.c } << 24) |
           (irt::u64{ s.v } << 25) | (irt::u64{ s.n } << 26) |
           (irt::u64{ s.z } << 27) | (irt::u64{ sync } << 28);
}

/** A 6502 reduced to the immediate operations of the ALU, STA abs and JMP abs,
 * with the cycles of the 6502: one memory access per cycle. The program
 * starts at address 0. A one-hot sequencer of flip-flops t1, t2 and t3
 * follows the fetch cycle t0, when none of them is true:
 * - t0: ir takes the opcode, pc + 1;
 * - t1: the immediate operation executes, or adl takes the low byte of the
 *   address; pc + 1;
 * - t2: the jump loads pc, or adh takes the high byte of the address and
 *   pc + 1;
 * - t3: the memory writes A at the address.
 *
 * The decoder only distinguishes the opcodes above. Outputs: pc0..pc15,
 * a0..a7, c, v, n, z then sync, true in t0. */
circuit make_cpu(irt::component_access& ids,
                 const circuit&         pc,
                 const circuit&         memory,
                 const circuit&         exec,
                 const circuit&         reg) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(0, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(29, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.clk   = c.get_or_add_x("clk");

    netlist    n(gen);
    const auto clk = netlist::input(*ret.clk);
    const auto p   = n.child(pc.id);
    const auto mem = n.child(memory.id);
    const auto eu  = n.child(exec.id);
    const auto ir  = n.child(reg.id);
    const auto adl = n.child(reg.id);
    const auto adh = n.child(reg.id);

    const auto bit = [](const irt::child_id child, const circuit& of,
                        const int i) {
        return netlist::output_of(child, of.y[i]);
    };

    std::array<wire, 3> t;
    for (auto& ff : t)
        ff = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto t0 = n.not_(n.any(t));

    // Among the opcodes run, bit 2 tells the absolute addresses from the
    // immediate operations, and bit 0 STA from JMP.
    const auto abs = bit(ir, reg, 2);
    const auto imm = n.not_(abs);
    const auto sta = n.and_(abs, bit(ir, reg, 0));
    const auto jmp = n.and_(abs, n.not_(bit(ir, reg, 0)));

    const auto load_adl = n.and_(t[0], abs);
    const auto load_adh = n.and_(t[1], sta);
    const auto jump     = n.and_(t[1], jmp);

    const std::array<wire, 3> next{ t0, load_adl, load_adh };
    for (int i = 0; i < 3; ++i) {
        n.link(next[i], *t[i].child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *t[i].child, mdl(irt::logical_d_flipflop::port_clk));
    }

    // The registers: d0..d7 then load.
    const std::array<std::pair<irt::child_id, wire>, 3> registers{
        std::pair{ ir, t0 }, std::pair{ adl, load_adl },
        std::pair{ adh, load_adh }
    };
    for (const auto& [r, load] : registers) {
        for (int i = 0; i < 8; ++i)
            n.link(bit(mem, memory, i), r, compo(reg.x[i]));
        n.link(load, r, compo(reg.x[8]));
        n.link(clk, r, compo(*reg.clk));
    }

    // The program counter: d0..d15, load then inc.
    for (int i = 0; i < 8; ++i) {
        n.link(bit(adl, reg, i), p, compo(pc.x[i]));
        n.link(bit(mem, memory, i), p, compo(pc.x[8 + i]));
    }
    n.link(jump, p, compo(pc.x[16]));
    // pc + 1 in each cycle but t3: in t2, the load of a jump wins.
    n.link(n.not_(t[2]), p, compo(pc.x[17]));
    n.link(clk, p, compo(*pc.clk));

    // The execution unit: b0..b7, op0..op2, load_a, load_c, load_v then
    // load_nz. The operation is the bits 7..5 of the opcode, except LDA (101)
    // which becomes pass_b (111) and SBC (111) which becomes sbc (100).
    const auto ir5     = bit(ir, reg, 5);
    const auto ir6     = bit(ir, reg, 6);
    const auto ir7     = bit(ir, reg, 7);
    const auto execute = n.and_(t[0], imm);
    const auto carry   = n.and_(execute, n.and_(ir6, ir5));

    for (int i = 0; i < 8; ++i)
        n.link(bit(mem, memory, i), eu, compo(exec.x[i]));
    n.link(n.and_(ir5, n.not_(n.and_(ir7, ir6))), eu, compo(exec.x[8]));
    n.link(n.xor_(ir7, ir6), eu, compo(exec.x[9]));
    n.link(ir7, eu, compo(exec.x[10]));
    n.link(execute, eu, compo(exec.x[11]));
    n.link(carry, eu, compo(exec.x[12]));
    n.link(carry, eu, compo(exec.x[13]));
    n.link(execute, eu, compo(exec.x[14]));
    n.link(clk, eu, compo(*exec.clk));

    // The memory: a0..a15, d0..d7 then we. The address is pc, or adh and adl
    // when A is written.
    const auto write = t[2];
    const auto read  = n.not_(write);
    for (int i = 0; i < 16; ++i) {
        const auto ad = i < 8 ? bit(adl, reg, i) : bit(adh, reg, i - 8);
        n.link(n.or_(n.and_(write, ad), n.and_(read, bit(p, pc, i))), mem,
               compo(memory.x[i]));
    }
    for (int i = 0; i < 8; ++i)
        n.link(bit(eu, exec, i), mem, compo(memory.x[16 + i]));
    n.link(write, mem, compo(memory.x[24]));
    n.link(clk, mem, compo(*memory.clk));

    for (int i = 0; i < 16; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("pc{}", i).sv())),
          bit(p, pc, i));
    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("a{}", i).sv())),
          bit(eu, exec, i));

    constexpr std::array<const char*, 4> flags{ "c", "v", "n", "z" };
    for (int i = 0; i < 4; ++i)
        n.output(*ret.y.emplace_back(c.get_or_add_y(flags[i])),
                 bit(eu, exec, 8 + i));

    n.output(*ret.y.emplace_back(c.get_or_add_y("sync")), t0);

    n.place();

    return ret;
}

/** The root of a circuit: one constant per input bit and one counter per
 * output bit, the counter keeps the last value received. The constants are
 * named x0, x1... and the counters y0, y1... to find them in a component read
 * from a file. */
struct bench {
    irt::component_id          id;
    irt::vector<irt::child_id> inputs;
    irt::vector<irt::child_id> outputs;
};

/** The clock of a bench: a rising edge at each odd date, the outputs are read
 * at the falling edges, once the circuit is stable. */
constexpr irt::real clock_period = 2.0;

bench make_bench(irt::component_access& ids, const circuit& tested) noexcept
{
    bench ret{ .id      = ids.alloc_generic_component(),
               .inputs  = irt::vector<irt::child_id>(tested.x.ssize(),
                                                     irt::reserve_tag),
               .outputs = irt::vector<irt::child_id>(tested.y.ssize(),
                                                     irt::reserve_tag) };

    auto& gen = ids.generic_components.get(
      ids.components[ret.id].id.generic_id);
    netlist n(gen);

    const auto dut = n.child(tested.id);

    for (const auto x : tested.x) {
        const auto cst = n.gate(dynamics_type::constant, {});
        n.link(cst, dut, compo(x));
        gen.children_names[*cst.child] = irt::format_n<8>("x{}",
                                                          ret.inputs.ssize())
                                           .sv();
        ret.inputs.emplace_back(*cst.child);
    }

    if (tested.clk) {
        const auto clk = n.gate(dynamics_type::logical_clock, {});
        gen.children_parameters[*clk.child].set_logical_clock(clock_period,
                                                              clock_period / 2);
        gen.children_names[*clk.child] = "clk";
        n.link(clk, dut, compo(*tested.clk));
    }

    for (const auto y : tested.y) {
        const auto cnt = n.gate(dynamics_type::counter,
                                { netlist::output_of(dut, y) });
        gen.children_names[*cnt.child] = irt::format_n<8>("y{}",
                                                          ret.outputs.ssize())
                                           .sv();
        ret.outputs.emplace_back(*cnt.child);
    }

    n.place();

    return ret;
}

/** Builds the simulation of the bench once: each evaluation only changes the
 * constants and initializes the simulation again. */
class bench_simulation
{
public:
    bench_simulation(irt::modeling& mod, const bench& b) noexcept
      : m_bench(b)
    {
        mod.ids.read([&](const auto& ids, auto) {
            mod.files.read([&](const auto& fs, auto) {
                boost::ut::expect(
                  boost::ut::fatal(m_pj.set(ids, fs, b.id).has_value()));
            });
        });
    }

    irt::u64 evaluate(const irt::u64 inputs) noexcept
    {
        start(inputs, 0);
        do {
            boost::ut::expect(boost::ut::fatal(m_pj.sim.run().has_value()));
        } while (not m_pj.sim.current_time_expired());

        return outputs();
    }

    /** Initializes the simulation of a sequential circuit for @c cycles
     * clock cycles. */
    void start(const irt::u64 inputs, const int cycles) noexcept
    {
        const auto& head = *m_pj.tree_nodes.try_to_get(m_pj.tn_head());

        for (int i = 0, e = m_bench.inputs.ssize(); i != e; ++i)
            m_pj.sim.parameters[head.children[m_bench.inputs[i]].mdl]
              .set_constant(static_cast<irt::real>((inputs >> i) & 1u));

        m_cycle = 0;
        m_pj.sim.limits.set_bound(0, (cycles + 1) * clock_period);
        boost::ut::expect(boost::ut::fatal(m_pj.sim.initialize().has_value()));
    }

    /** Changes the inputs at the current date: a sequential circuit reads
     * them at the next rising edge. */
    void set_inputs(const irt::u64 inputs) noexcept
    {
        const auto& head = *m_pj.tree_nodes.try_to_get(m_pj.tn_head());

        for (int i = 0, e = m_bench.inputs.ssize(); i != e; ++i)
            boost::ut::expect(!!m_pj.sim.send_value(
              head.children[m_bench.inputs[i]].mdl,
              static_cast<irt::real>((inputs >> i) & 1u)));
    }

    /** Runs the simulation to the falling edge after the next rising edge
     * and returns the outputs. */
    irt::u64 cycle() noexcept
    {
        const auto falling_edge = static_cast<irt::real>(++m_cycle) *
                                  clock_period;

        while (m_pj.sim.current_time() < falling_edge)
            boost::ut::expect(boost::ut::fatal(m_pj.sim.run().has_value()));

        return outputs();
    }

    /** The bytes of the only memory of the simulation. */
    std::span<const irt::u8> memory() const noexcept
    {
        boost::ut::expect(boost::ut::fatal(m_pj.sim.memories.ssize() == 1));
        const auto& bank = *m_pj.sim.memories.begin();
        return std::span(bank.bytes.data(), bank.bytes.size());
    }

private:
    irt::u64 outputs() noexcept
    {
        const auto& head = *m_pj.tree_nodes.try_to_get(m_pj.tn_head());

        irt::u64 outputs = 0;
        for (int i = 0, e = m_bench.outputs.ssize(); i != e; ++i) {
            const auto& cnt = irt::get_dyn<irt::counter>(
              m_pj.sim.models.get(head.children[m_bench.outputs[i]].mdl));
            if (cnt.last_value != 0.0)
                outputs |= irt::u64{ 1 } << i;
        }

        return outputs;
    }

    irt::project m_pj;
    const bench& m_bench;
    int          m_cycle = 0;
};

/** Saves the components in the directory @c dir/digital, in the order of
 * their dependencies: a file references the files of its children. */
void save_circuits(
  irt::modeling&                                                   mod,
  const std::string_view                                           dir,
  std::initializer_list<std::pair<irt::component_id, const char*>> files)
{
    const auto dir_id = mod.files.write([&](auto& fs) {
        const auto reg                   = fs.alloc_registred("circuits", 0);
        fs.registred_paths.get(reg).path = dir;

        const auto id               = fs.alloc_dir(reg);
        fs.dir_paths.get(id).parent = reg;
        fs.dir_paths.get(id).path   = "digital";

        (void)fs.create_directories(reg);
        (void)fs.create_directories(id);
        return id;
    });

    for (const auto& [compo_id, name] : files) {
        const auto file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, name, irt::file_type::component_file);
        });

        mod.ids.write([&](auto& ids) {
            ids.component_file_paths[compo_id].file = file_id;
            mod.files.read([&](const auto& fs, auto) {
                boost::ut::expect(
                  boost::ut::fatal(mod.save(ids, fs, compo_id).has_value()));
            });
        });
    }
}

/** Reads every component of the directory @c dir and returns the bench stored
 * in @c file. */
bench load_bench(irt::modeling&         mod,
                 const std::string_view dir,
                 const std::string_view file,
                 const int              inputs,
                 const int              outputs)
{
    mod.files.write([&](auto& fs) {
        const auto reg                   = fs.alloc_registred("circuits", 0);
        fs.registred_paths.get(reg).path = dir;
    });

    boost::ut::expect(boost::ut::fatal(mod.fill_components().has_value()));

    bench ret{ .id      = irt::undefined<irt::component_id>(),
               .inputs  = irt::vector<irt::child_id>(inputs, irt::reserve_tag),
               .outputs = irt::vector<irt::child_id>(outputs,
                                                     irt::reserve_tag) };

    mod.ids.read([&](const auto& ids, auto) {
        mod.files.read([&](const auto& fs, auto) {
            for (const auto id : ids) {
                boost::ut::expect(
                  irt::is_loaded(ids.components[id].file_status));

                const auto* f = fs.file_paths.try_to_get(
                  ids.component_file_paths[id].file);
                if (f and f->path.sv() == file)
                    ret.id = id;
            }
        });

        boost::ut::expect(boost::ut::fatal(ids.exists(ret.id)));
        const auto& gen = ids.generic_components.get(
          ids.components[ret.id].id.generic_id);

        const auto find = [&](const char prefix, const int i) {
            const auto name = irt::format_n<8>("{}{}", prefix, i);
            for (const auto& child : gen.children) {
                const auto id = gen.children.get_id(child);
                if (gen.children_names[id].sv() == name.sv())
                    return id;
            }

            boost::ut::expect(boost::ut::fatal(false)) << name.sv();
            return irt::undefined<irt::child_id>();
        };

        for (int i = 0; i < inputs; ++i)
            ret.inputs.emplace_back(find('x', i));
        for (int i = 0; i < outputs; ++i)
            ret.outputs.emplace_back(find('y', i));
    });

    return ret;
}

/** Checks the ALU against @c alu_reference: every pair of the edge values and
 * random pairs, for each operation and each carry. */
irt::vector<irt::u8> random_bytes(const int size, const unsigned seed) noexcept
{
    irt::vector<irt::u8> bytes(size, irt::reserve_tag);
    std::mt19937         gen(seed);
    for (int i = 0; i < size; ++i)
        bytes.emplace_back(static_cast<irt::u8>(gen() & 0xffu));

    return bytes;
}

/** Reads the first addresses, beyond the @a content too, without writing. */
void check_memory_content(bench_simulation&              sim,
                          const std::span<const irt::u8> content) noexcept
{
    const int addresses = static_cast<int>(content.size()) + 100;
    sim.start(0, addresses);

    for (int address = 0; address < addresses; ++address) {
        sim.set_inputs(static_cast<irt::u64>(address));
        const auto expected = address < std::ssize(content)
                                ? content[static_cast<std::size_t>(address)]
                                : irt::u8{ 0 };

        boost::ut::expect(
          boost::ut::eq(sim.cycle(), static_cast<irt::u64>(expected)))
          << "address " << address;
    }
}

bench make_cpu_bench(irt::component_access&   ids,
                     std::span<const irt::u8> program) noexcept
{
    const auto fa  = make_full_adder(ids);
    const auto alu = make_alu(ids, make_adder<8>(ids, fa));
    const auto reg = make_register<8>(ids);
    const auto acc = make_accumulator(ids, alu, reg);
    const auto eu  = make_execution_unit(ids, acc, make_flags(ids));

    return make_bench(ids, make_cpu(ids, make_program_counter(ids),
                                    make_memory(ids, program), eu, reg));
}

/** Runs @a instructions instructions of the @a program on the CPU and on the
 * reference, cycle by cycle: sync is true only on the first cycle of an
 * instruction, when the registers show the results of the previous ones. */
void check_cpu(bench_simulation&        sim,
               std::span<const irt::u8> program,
               const int                instructions) noexcept
{
    irt::vector<irt::u8> mem(irt::memory_bank::size, irt::u8{ 0 });
    std::copy(program.begin(), program.end(), mem.begin());
    const auto bytes = std::span(mem.data(), mem.size());

    auto dry_run = mem;
    int  cycles  = 0;
    for (cpu_state s; const auto i : std::views::iota(0, instructions)) {
        (void)i;
        cycles += cpu_reference(s, std::span(dry_run.data(), dry_run.size()));
    }

    cpu_state ref;
    sim.start(0, cycles);
    for (int i = 0, k = 0; i < instructions; ++i) {
        const auto pc       = ref.pc;
        const auto duration = cpu_reference(ref, bytes);

        for (int j = 1; j <= duration; ++j) {
            const auto out = sim.cycle();
            ++k;

            if (j < duration)
                boost::ut::expect(boost::ut::eq((out >> 28) & 1u, 0u))
                  << "cycle " << k << " instruction at " << pc;
            else
                boost::ut::expect(boost::ut::eq(out, cpu_outputs(ref, 1)))
                  << "cycle " << k << " after instruction at " << pc;
        }
    }

    const auto memory = sim.memory();
    boost::ut::expect(boost::ut::fatal(
      boost::ut::eq(memory.size(), static_cast<std::size_t>(mem.size()))));
    for (int i = 0; i < mem.ssize(); ++i)
        if (memory[i] != mem[i]) {
            boost::ut::expect(boost::ut::eq(static_cast<int>(memory[i]),
                                            static_cast<int>(mem[i])))
              << "address " << i;
            break;
        }
}

void check_alu(bench_simulation& sim)
{
    constexpr std::array<unsigned, 5> edges{ 0x00, 0x01, 0x7f, 0x80, 0xff };
    std::mt19937                      gen(12345u);

    const auto check = [&](const alu_op op, const unsigned x, const unsigned y,
                           const unsigned cin) {
        const auto expected = alu_outputs(alu_reference(op, x, y, cin));
        boost::ut::expect(
          boost::ut::eq(sim.evaluate(alu_inputs(op, x, y, cin)), expected))
          << "op " << static_cast<unsigned>(op) << " a " << x << " b " << y
          << " cin " << cin;
    };

    for (unsigned o = 0; o < alu_op_count; ++o) {
        const auto op = static_cast<alu_op>(o);
        for (unsigned cin = 0; cin < 2; ++cin) {
            for (const auto x : edges)
                for (const auto y : edges)
                    check(op, x, y, cin);

            for (int i = 0; i < 64; ++i)
                check(op, gen() & 0xffu, gen() & 0xffu, cin);
        }
    }
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

    "d-flipflop-samples-d-before-the-rising-edge"_test = [] {
        irt::simulation sim;
        auto&           ff = sim.alloc<irt::logical_d_flipflop>();

        // Each constant sends its value at its date: the d and clk signals.
        const auto drive = [&](const int port, const irt::real value,
                               const irt::real date) {
            auto& cst = sim.alloc<irt::constant>();
            sim.parameters[sim.get_id(cst)].set_constant(value, date);
            expect(!!sim.connect_dynamics(cst, 0, ff, port));
        };

        using irt::logical_d_flipflop;
        drive(logical_d_flipflop::port_d, 1, 0.5);
        drive(logical_d_flipflop::port_clk, 1, 1);
        drive(logical_d_flipflop::port_d, 0, 1.5);
        drive(logical_d_flipflop::port_clk, 0, 2);
        drive(logical_d_flipflop::port_clk, 1, 3);
        drive(logical_d_flipflop::port_d, 1, 3);
        drive(logical_d_flipflop::port_clk, 0, 4);
        drive(logical_d_flipflop::port_clk, 1, 5);
        drive(logical_d_flipflop::port_d, 0, 5.5);
        drive(logical_d_flipflop::port_clk, 1, 6);

        std::array<std::pair<irt::real, bool>, 8> changes{};
        int                                       count = 0;
        bool                                      q     = false;

        sim.limits.set_bound(0, 10);
        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
            if (ff.q != q and count < 8) {
                q                = ff.q;
                changes[count++] = { sim.current_time(), q };
            }
        } while (not sim.current_time_expired());

        // At 3, d rises with the clock: q keeps the value of d before 3. At
        // 6, clk stays true: there is no rising edge.
        expect(fatal(eq(count, 3)));
        expect(eq(changes[0].first, 1.0) and changes[0].second);
        expect(eq(changes[1].first, 3.0) and not changes[1].second);
        expect(eq(changes[2].first, 5.0) and changes[2].second);
    };

    "clock-toggles-every-half-period-after-its-offset"_test = [] {
        irt::simulation sim;
        auto&           clk = sim.alloc<irt::logical_clock>();
        sim.parameters[sim.get_id(clk)].set_logical_clock(2.0, 1.0);

        std::array<std::pair<irt::real, bool>, 6> changes{};
        int                                       count = 0;
        bool                                      value = false;

        sim.limits.set_bound(0, 10);
        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
            if (clk.value != value and count < 6) {
                value            = clk.value;
                changes[count++] = { sim.current_time(), value };
            }
        } while (not sim.current_time_expired());

        expect(fatal(eq(count, 6)));
        for (int i = 0; i < 6; ++i) {
            expect(eq(changes[i].first, 1.0 + i));
            expect(changes[i].second == (i % 2 == 0));
        }
    };

    "clock-refuses-a-null-period"_test = [] {
        irt::simulation sim;
        auto&           clk = sim.alloc<irt::logical_clock>();
        sim.parameters[sim.get_id(clk)].set_logical_clock(0.0);

        const auto callback    = std::exchange(irt::on_error_callback, nullptr);
        const auto ret         = sim.initialize();
        irt::on_error_callback = callback;

        expect(fatal(not ret.has_value()));
        expect(ret.error() ==
               irt::error_code(irt::simulation_errc::clock_period_error));
    };

    "flipflop-fed-by-its-inverse-divides-the-clock-by-two"_test = [] {
        irt::simulation sim;
        auto&           clk = sim.alloc<irt::logical_clock>();
        auto&           ff  = sim.alloc<irt::logical_d_flipflop>();
        auto&           inv = sim.alloc<irt::logical_invert>();
        sim.parameters[sim.get_id(clk)].set_logical_clock(2.0, 1.0);

        using irt::logical_d_flipflop;
        expect(
          !!sim.connect_dynamics(clk, 0, ff, logical_d_flipflop::port_clk));
        expect(!!sim.connect_dynamics(ff, 0, inv, 0));
        expect(!!sim.connect_dynamics(inv, 0, ff, logical_d_flipflop::port_d));

        // The rising edges are at 1, 3, 5, 7 and 9: q toggles at each one.
        int  toggles = 0;
        bool q       = false;

        sim.limits.set_bound(0, 10);
        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
            if (ff.q != q) {
                q = ff.q;
                ++toggles;
                expect(eq(std::fmod(sim.current_time(), 2.0), 1.0));
            }
        } while (not sim.current_time_expired());

        expect(eq(toggles, 5));
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

        const auto b = mod.ids.write([&](auto& ids) {
            const auto fa = make_full_adder(ids);
            return make_bench(ids, as_circuit(make_adder<4>(ids, fa)));
        });

        bench_simulation sim(mod, b);

        for (irt::u64 cin = 0; cin < 2; ++cin)
            for (irt::u64 x = 0; x < 16; ++x)
                for (irt::u64 y = 0; y < 16; ++y)
                    expect(
                      eq(sim.evaluate(x | (y << 4) | (cin << 8)), x + y + cin))
                      << x << " + " << y << " + " << cin;
    };

    "alu-8-bits-follows-the-6502-operations"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write([&](auto& ids) {
            const auto fa = make_full_adder(ids);
            return make_bench(ids, make_alu(ids, make_adder<8>(ids, fa)));
        });

        bench_simulation sim(mod, b);
        check_alu(sim);
    };

    "counter-8-bits-counts-the-rising-edges"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write(
          [&](auto& ids) { return make_bench(ids, make_counter<8>(ids)); });

        bench_simulation sim(mod, b);

        constexpr int cycles = 300;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k)
            expect(eq(sim.cycle(), static_cast<irt::u64>(k % 256)))
              << "cycle " << k;
    };

    "register-8-bits-loads-only-when-load-is-true"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write(
          [&](auto& ids) { return make_bench(ids, make_register<8>(ids)); });

        bench_simulation sim(mod, b);
        std::mt19937     gen(4321u);

        constexpr int cycles = 200;
        irt::u64      q      = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const irt::u64 d    = gen() & 0xffu;
            const irt::u64 load = gen() & 1u;
            sim.set_inputs(d | (load << 8));
            if (load)
                q = d;

            expect(eq(sim.cycle(), q)) << "cycle " << k;
        }
    };

    "accumulator-loads-the-result-of-the-alu"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write([&](auto& ids) {
            const auto fa  = make_full_adder(ids);
            const auto alu = make_alu(ids, make_adder<8>(ids, fa));
            const auto reg = make_register<8>(ids);
            return make_bench(ids, make_accumulator(ids, alu, reg));
        });

        bench_simulation sim(mod, b);
        std::mt19937     gen(2468u);

        constexpr int cycles = 300;
        unsigned      a      = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const auto op   = static_cast<alu_op>(gen() % alu_op_count);
            const auto x    = gen() & 0xffu;
            const auto cin  = gen() & 1u;
            const auto load = gen() & 1u;

            sim.set_inputs(x | (cin << 8) | (static_cast<irt::u64>(op) << 9) |
                           (static_cast<irt::u64>(load) << 12));
            if (load)
                a = alu_reference(op, a, x, cin).r;

            // The flags are combinational: the ALU result for the new A.
            const auto flags = alu_outputs(alu_reference(op, a, x, cin)) >> 8;
            expect(eq(sim.cycle(), a | (flags << 8)))
              << "cycle " << k << " op " << static_cast<unsigned>(op);
        }
    };

    "execution-unit-keeps-the-flags-of-the-loaded-results"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write([&](auto& ids) {
            const auto fa  = make_full_adder(ids);
            const auto alu = make_alu(ids, make_adder<8>(ids, fa));
            const auto acc = make_accumulator(ids, alu, make_register<8>(ids));
            return make_bench(ids,
                              make_execution_unit(ids, acc, make_flags(ids)));
        });

        bench_simulation sim(mod, b);
        std::mt19937     gen(1357u);

        constexpr int cycles = 400;
        unsigned      a = 0, c = 0, v = 0, n = 0, z = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const auto     op    = static_cast<alu_op>(gen() % alu_op_count);
            const auto     x     = gen() & 0xffu;
            const irt::u64 loads = gen() & 0xfu;

            sim.set_inputs(x | (static_cast<irt::u64>(op) << 8) |
                           (loads << 11));

            const auto res = alu_reference(op, a, x, c);
            if (loads & 1u)
                a = res.r;
            if (loads & 2u)
                c = res.c;
            if (loads & 4u)
                v = res.v;
            if (loads & 8u) {
                n = res.r >> 7;
                z = res.r == 0 ? 1u : 0u;
            }

            expect(
              eq(sim.cycle(), a | (c << 8) | (v << 9) | (n << 10) | (z << 11)))
              << "cycle " << k << " op " << static_cast<unsigned>(op)
              << " loads " << loads;
        }
    };

    "program-counter-increments-and-jumps"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write([&](auto& ids) {
            return make_bench(ids, make_program_counter(ids));
        });

        bench_simulation sim(mod, b);
        std::mt19937     gen(9753u);

        // Some jumps go just before 0xffff to check the wrap to 0.
        constexpr int cycles = 500;
        irt::u64      pc     = 0;
        int           wraps  = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const irt::u64 d    = gen() % 4 == 0 ? 0xfffeu : gen() & 0xffffu;
            const irt::u64 load = gen() % 8 == 0 ? 1u : 0u;
            const irt::u64 inc  = gen() % 4 != 0 ? 1u : 0u;

            sim.set_inputs(d | (load << 16) | (inc << 17));
            if (not load and inc and pc == 0xffffu)
                ++wraps;
            pc = load ? d : inc ? (pc + 1) & 0xffffu : pc;

            expect(eq(sim.cycle(), pc))
              << "cycle " << k << " load " << load << " inc " << inc;
        }

        expect(gt(wraps, 0));
    };

    "memory-reads-what-was-written"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write(
          [&](auto& ids) { return make_bench(ids, make_memory(ids)); });

        bench_simulation sim(mod, b);
        std::mt19937     gen(8642u);

        // Few addresses: most of the reads find a written byte.
        irt::vector<irt::u8>     bytes(irt::memory_bank::size, irt::u8{ 0 });
        std::array<irt::u64, 16> addresses{};
        for (auto& address : addresses)
            address = gen() & 0xffffu;

        constexpr int cycles = 600;
        int           writes = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const auto     address = addresses[gen() % addresses.size()];
            const irt::u64 data    = gen() & 0xffu;
            const irt::u64 we      = gen() % 3 == 0 ? 1u : 0u;

            sim.set_inputs(address | (data << 16) | (we << 24));
            if (we) {
                bytes[static_cast<int>(address)] = static_cast<irt::u8>(data);
                ++writes;
            }

            expect(eq(sim.cycle(),
                      static_cast<irt::u64>(bytes[static_cast<int>(address)])))
              << "cycle " << k << " address " << address << " we " << we;
        }

        expect(gt(writes, 0));
    };

    "memory-writes-with-the-inputs-before-the-rising-edge"_test = [] {
        irt::simulation sim;
        auto&           mem = sim.alloc<irt::logical_memory>();

        const auto drive = [&](const int port, const irt::real value,
                               const irt::real date) {
            auto& cst = sim.alloc<irt::constant>();
            sim.parameters[sim.get_id(cst)].set_constant(value, date);
            expect(!!sim.connect_dynamics(cst, 0, mem, port));
        };

        // At 1, we rises with the clock: the write uses we before 1, so
        // nothing is written. At 3, we is true before the edge.
        using irt::logical_memory;
        drive(logical_memory::port_d, 1, 0.5);
        drive(logical_memory::port_we, 1, 1);
        drive(logical_memory::port_clk, 1, 1);
        drive(logical_memory::port_clk, 0, 2);
        drive(logical_memory::port_clk, 1, 3);

        irt::real changed_at = -1;
        sim.limits.set_bound(0, 10);
        expect(fatal(sim.initialize().has_value()));
        do {
            expect(fatal(sim.run().has_value()));
            if (mem.out != 0 and changed_at < 0)
                changed_at = sim.current_time();
        } while (not sim.current_time_expired());

        expect(eq(changed_at, 3.0));
        expect(eq(mem.out, irt::u8{ 1 }));
    };

    "memory-frees-its-bytes-with-its-model"_test = [] {
        irt::simulation sim;
        auto&           mem = sim.alloc<irt::logical_memory>();

        sim.limits.set_bound(0, 1);
        expect(fatal(sim.initialize().has_value()));
        expect(eq(sim.memories.ssize(), 1));

        sim.deallocate(sim.get_id(mem));
        expect(eq(sim.memories.ssize(), 0));
    };

    "memory-starts-with-its-content"_test = [] {
        const auto    content = random_bytes(300, 1357u);
        irt::modeling mod;
        const auto    b = mod.ids.write([&](auto& ids) {
            return make_bench(ids, make_memory(ids, content));
        });

        bench_simulation sim(mod, b);
        check_memory_content(sim, content);
    };

    "memory-loads-its-content-at-each-initialization"_test = [] {
        irt::simulation sim;
        const irt::real values[] = { 0xa9, 0x2a, 0x8d };
        expect(fatal(sim.srcs.constant_sources.reserve(1)));
        auto& cst = sim.srcs.constant_sources.alloc(values);
        auto& mem = sim.alloc<irt::logical_memory>();
        sim.parameters[sim.get_id(mem)].set_logical_memory(
          irt::source_type::constant, sim.srcs.constant_sources.get_id(cst), 5);

        // The source restarts when it is empty.
        const irt::u8 expected[] = { 0xa9, 0x2a, 0x8d, 0xa9, 0x2a, 0 };

        sim.limits.set_bound(0, 1);
        for (int run = 0; run < 2; ++run) {
            expect(fatal(sim.initialize().has_value()));

            const auto& bank = *sim.memories.try_to_get(mem.id);
            for (int i = 0; i < 6; ++i)
                expect(eq(static_cast<int>(bank.bytes[i]),
                          static_cast<int>(expected[i])))
                  << "run " << run << " byte " << i;

            expect(eq(static_cast<int>(mem.out), 0xa9));
            expect(eq(mem.sigma, irt::time_domain<irt::time>::zero));
            expect(sim.finalize().has_value());
        }
    };

    "memory-refuses-an-invalid-content"_test = [] {
        const auto initialize = [](const irt::real value, const irt::i32 size) {
            irt::simulation sim;
            const irt::real values[] = { value };
            expect(fatal(sim.srcs.constant_sources.reserve(1)));
            auto& cst = sim.srcs.constant_sources.alloc(values);
            auto& mem = sim.alloc<irt::logical_memory>();
            sim.parameters[sim.get_id(mem)].set_logical_memory(
              irt::source_type::constant, sim.srcs.constant_sources.get_id(cst),
              size);

            sim.limits.set_bound(0, 1);
            return sim.initialize().has_value();
        };

        expect(initialize(255, 1));
        expect(initialize(255, irt::memory_bank::size));
        expect(not initialize(256, 1));
        expect(not initialize(-1, 1));
        expect(not initialize(1.5, 1));
        expect(not initialize(1, irt::memory_bank::size + 1));
        expect(not initialize(1, -1));
    };

    "memory-content-survives-a-round-trip-through-irt-files"_test = [] {
        const auto* keep = std::getenv("IRRITATOR_CIRCUITS_DIR");
        const auto  temp = irt::temp_path_with_unlink{ "memory-content" };
        expect(fatal(temp.success()));
        const std::string_view dir = keep ? keep : temp.c_str();

        const auto content = random_bytes(200, 2468u);
        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto mem = make_memory(ids, content);
                const auto b   = make_bench(ids, mem);
                return std::array{ mem.id, b.id };
            });

            save_circuits(
              mod, dir,
              { { compo[0], "memory.irt" }, { compo[1], "memory-bench.irt" } });
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "memory-bench.irt", 25, 8);
        bench_simulation sim(mod, b);
        check_memory_content(sim, content);
    };

    "cpu-runs-a-program-of-each-instruction"_test = [] {
        // 0x40 + 0x40 overflows, 0x80 - 1 with the borrow of the carry.
        constexpr std::array<irt::u8, 26> program{
            0xa9, 0x40,       // LDA #$40
            0x69, 0x40,       // ADC #$40
            0x8d, 0x00, 0x80, // STA $8000
            0xe9, 0x01,       // SBC #$01
            0x8d, 0x01, 0x80, // STA $8001
            0x49, 0xff,       // EOR #$FF
            0x29, 0x0f,       // AND #$0F
            0x09, 0x80,       // ORA #$80
            0x8d, 0x02, 0xc0, // STA $C002
            0x4c, 0x00, 0x00, // JMP $0000
            0x00, 0x00
        };

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        check_cpu(sim, program, 40);
    };

    "cpu-runs-random-programs"_test = [] {
        for (const auto seed : { 2024u, 6502u, 1975u }) {
            std::mt19937         gen(seed);
            irt::vector<irt::u8> program(512, irt::reserve_tag);
            irt::vector<int>     starts(64, irt::reserve_tag);

            const auto emit = [&](const opcode op, const unsigned operand,
                                  const bool absolute) {
                starts.emplace_back(program.ssize());
                program.emplace_back(static_cast<irt::u8>(op));
                program.emplace_back(static_cast<irt::u8>(operand & 0xffu));
                if (absolute)
                    program.emplace_back(static_cast<irt::u8>(operand >> 8));
            };

            // The jumps go backward, to an instruction already emitted. The
            // writes stay above the program.
            for (int i = 0; i < 40; ++i) {
                const auto kind = gen() % 8;
                if (kind < 5)
                    emit(immediate_opcodes[gen() % immediate_opcodes.size()],
                         gen() & 0xffu, false);
                else if (kind < 7 or starts.empty())
                    emit(opcode::sta_abs, 0x1000u + (gen() & 0xefffu), true);
                else
                    emit(opcode::jmp_abs,
                         static_cast<unsigned>(starts[gen() % starts.size()]),
                         true);
            }
            emit(opcode::jmp_abs, 0u, true);

            irt::modeling mod;
            const auto    b = mod.ids.write(
              [&](auto& ids) { return make_cpu_bench(ids, program); });

            bench_simulation sim(mod, b);
            check_cpu(sim, program, 150);
        }
    };

    "cpu-survives-a-round-trip-through-irt-files"_test = [] {
        const auto* keep = std::getenv("IRRITATOR_CIRCUITS_DIR");
        const auto  temp = irt::temp_path_with_unlink{ "cpu" };
        expect(fatal(temp.success()));
        const std::string_view dir = keep ? keep : temp.c_str();

        // LDA #$01, ADC #$01, STA $2000 then JMP $0002: A counts by one.
        constexpr std::array<irt::u8, 10> program{
            0xa9, 0x01, 0x69, 0x01, 0x8d, 0x00, 0x20, 0x4c, 0x02, 0x00
        };
        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto fa    = make_full_adder(ids);
                const auto adder = make_adder<8>(ids, fa);
                const auto alu   = make_alu(ids, adder);
                const auto reg   = make_register<8>(ids);
                const auto acc   = make_accumulator(ids, alu, reg);
                const auto flags = make_flags(ids);
                const auto eu    = make_execution_unit(ids, acc, flags);
                const auto pc    = make_program_counter(ids);
                const auto mem   = make_memory(ids, program);
                const auto cpu   = make_cpu(ids, pc, mem, eu, reg);
                const auto b     = make_bench(ids, cpu);
                return std::array{ fa.id,  adder.id, alu.id, reg.id,
                                   acc.id, flags.id, eu.id,  pc.id,
                                   mem.id, cpu.id,   b.id };
            });

            save_circuits(mod, dir,
                          { { compo[0], "full-adder.irt" },
                            { compo[1], "adder-8.irt" },
                            { compo[2], "alu-8.irt" },
                            { compo[3], "register-8.irt" },
                            { compo[4], "accumulator.irt" },
                            { compo[5], "flags.irt" },
                            { compo[6], "execution-unit.irt" },
                            { compo[7], "program-counter.irt" },
                            { compo[8], "cpu-memory.irt" },
                            { compo[9], "cpu.irt" },
                            { compo[10], "cpu-bench.irt" } });
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "cpu-bench.irt", 0, 29);
        bench_simulation sim(mod, b);
        check_cpu(sim, program, 60);
    };

    "alu-8-bits-survives-a-round-trip-through-irt-files"_test = [] {
        // IRRITATOR_CIRCUITS_DIR keeps the files to open them in the editor.
        const auto* keep = std::getenv("IRRITATOR_CIRCUITS_DIR");
        const auto  temp = irt::temp_path_with_unlink{ "digital-circuits" };
        expect(fatal(temp.success()));
        const std::string_view dir = keep ? keep : temp.c_str();

        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto fa    = make_full_adder(ids);
                const auto adder = make_adder<8>(ids, fa);
                const auto alu   = make_alu(ids, adder);
                const auto b     = make_bench(ids, alu);
                return std::array{ fa.id, adder.id, alu.id, b.id };
            });

            save_circuits(mod, dir,
                          { { compo[0], "full-adder.irt" },
                            { compo[1], "adder-8.irt" },
                            { compo[2], "alu-8.irt" },
                            { compo[3], "alu-bench.irt" } });
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "alu-bench.irt", 20, 12);
        bench_simulation sim(mod, b);
        check_alu(sim);
    };
}
