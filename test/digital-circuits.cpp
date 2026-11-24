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
        return tree(inputs, dynamics_type::logical_or_2,
                    dynamics_type::logical_or_3);
    }

    /** A tree of and gates: three inputs per gate as long as possible. */
    wire all(const std::span<const wire> inputs)
    {
        return tree(inputs, dynamics_type::logical_and_2,
                    dynamics_type::logical_and_3);
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
    wire tree(const std::span<const wire> inputs,
              const dynamics_type         two,
              const dynamics_type         three)
    {
        switch (inputs.size()) {
        case 1:
            return inputs[0];
        case 2:
            return gate(two, { inputs[0], inputs[1] });
        case 3:
            return gate(three, { inputs[0], inputs[1], inputs[2] });
        default: {
            const auto first  = (inputs.size() + 2) / 3;
            const auto second = (inputs.size() - first + 1) / 2;
            return gate(three,
                        { tree(inputs.subspan(0, first), two, three),
                          tree(inputs.subspan(first, second), two, three),
                          tree(inputs.subspan(first + second), two, three) });
        }
        }
    }

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

/** The index registers X and Y: on a rising edge, when write is true, the
 * register selected by sel_y takes d if imm is true, its value plus one
 * otherwise, or minus one if dec is true. Inputs: d0..d7, imm, dec, sel_y then
 * write. Outputs: x0..x7, y0..y7 then the n and z of the result. */
circuit make_index_registers(irt::component_access& ids,
                             const circuit&         reg) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(12, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(18, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    const auto input = [&](const std::string_view name) {
        return netlist::input(*ret.x.emplace_back(c.get_or_add_x(name)));
    };

    std::array<wire, 8> d;
    for (int i = 0; i < 8; ++i)
        d[i] = input(irt::format_n<8>("d{}", i).sv());
    const auto imm   = input("imm");
    const auto dec   = input("dec");
    const auto sel_y = input("sel_y");
    const auto write = input("write");
    ret.clk          = c.get_or_add_x("clk");
    const auto clk   = netlist::input(*ret.clk);

    netlist    n(gen);
    const auto x     = n.child(reg.id);
    const auto y     = n.child(reg.id);
    const auto sel_x = n.not_(sel_y);
    const auto step  = n.not_(imm);

    // v - 1 = not(not(v) + 1): the incrementer serves both directions.
    std::array<wire, 8> v;
    for (int i = 0; i < 8; ++i)
        v[i] = n.xor_(n.or_(n.and_(sel_y, netlist::output_of(y, reg.y[i])),
                            n.and_(sel_x, netlist::output_of(x, reg.y[i]))),
                      dec);
    const auto next = make_incrementer<8>(n, v);

    std::array<wire, 8> r;
    for (int i = 0; i < 8; ++i)
        r[i] = n.or_(n.and_(imm, d[i]), n.and_(step, n.xor_(next[i], dec)));

    // The registers: d0..d7 then load.
    const std::array registers{ std::pair{ x, n.and_(write, sel_x) },
                                std::pair{ y, n.and_(write, sel_y) } };
    for (const auto& [child, load] : registers) {
        for (int i = 0; i < 8; ++i)
            n.link(r[i], child, compo(reg.x[i]));
        n.link(load, child, compo(reg.x[8]));
        n.link(clk, child, compo(*reg.clk));
    }

    for (const auto& [child, name] :
         { std::pair{ x, 'x' }, std::pair{ y, 'y' } })
        for (int i = 0; i < 8; ++i)
            n.output(*ret.y.emplace_back(
                       c.get_or_add_y(irt::format_n<8>("{}{}", name, i).sv())),
                     netlist::output_of(child, reg.y[i]));

    n.output(*ret.y.emplace_back(c.get_or_add_y("n")), r[7]);
    n.output(*ret.y.emplace_back(c.get_or_add_y("z")), n.not_(n.any(r)));

    n.place();

    return ret;
}

/** The stack pointer S: on a rising edge, S takes d if load is true, S + 1 if
 * inc is true or S - 1 if dec is true. Inputs: d0..d7, load, inc then dec.
 * Outputs: s0..s7. */
circuit make_stack_pointer(irt::component_access& ids,
                           const circuit&         reg) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(11, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(8, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    const auto input = [&](const std::string_view name) {
        return netlist::input(*ret.x.emplace_back(c.get_or_add_x(name)));
    };

    std::array<wire, 8> d;
    for (int i = 0; i < 8; ++i)
        d[i] = input(irt::format_n<8>("d{}", i).sv());
    const auto load = input("load");
    const auto inc  = input("inc");
    const auto dec  = input("dec");
    ret.clk         = c.get_or_add_x("clk");

    netlist    n(gen);
    const auto s    = n.child(reg.id);
    const auto step = n.not_(load);

    // s - 1 = not(not(s) + 1), as for the index registers.
    std::array<wire, 8> v;
    for (int i = 0; i < 8; ++i)
        v[i] = n.xor_(netlist::output_of(s, reg.y[i]), dec);
    const auto next = make_incrementer<8>(n, v);

    // The register: d0..d7 then load.
    for (int i = 0; i < 8; ++i)
        n.link(n.or_(n.and_(load, d[i]), n.and_(step, n.xor_(next[i], dec))), s,
               compo(reg.x[i]));
    n.link(n.any(std::array{ load, inc, dec }), s, compo(reg.x[8]));
    n.link(netlist::input(*ret.clk), s, compo(*reg.clk));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("s{}", i).sv())),
          netlist::output_of(s, reg.y[i]));

    n.place();

    return ret;
}

/** The opcodes of the 6502 run by the CPU. The operations of the ALU, CMP,
 * STA, LDX, LDY, CPX and CPY exist with other addressing modes, see
 * with_mode(). */
enum class opcode : irt::u8 {
    ora_imm = 0x09,
    and_imm = 0x29,
    eor_imm = 0x49,
    adc_imm = 0x69,
    lda_imm = 0xa9,
    sbc_imm = 0xe9,
    cmp_imm = 0xc9,
    sta_abs = 0x8d,
    ldx_imm = 0xa2,
    ldy_imm = 0xa0,
    cpx_imm = 0xe0,
    cpy_imm = 0xc0,
    inx     = 0xe8,
    iny     = 0xc8,
    dex     = 0xca,
    dey     = 0x88,
    beq     = 0xf0,
    bne     = 0xd0,
    jmp_abs = 0x4c,
    pha     = 0x48,
    pla     = 0x68,
    jsr_abs = 0x20,
    rts     = 0x60,
    txs     = 0x9a,
    tsx     = 0xba,
    asl_a   = 0x0a,
    rol_a   = 0x2a,
    lsr_a   = 0x4a,
    ror_a   = 0x6a,
};

/** The addressing modes: the bits 4..2 of the opcodes of the ALU, CMP and STA,
 * whose bits 1..0 are 01. LDX, LDY, CPX and CPY use the same bits but 000 for
 * an immediate value. */
enum class addressing : irt::u8 {
    zero_page   = 0b001,
    immediate   = 0b010,
    absolute    = 0b011,
    zero_page_x = 0b101,
    absolute_y  = 0b110,
    absolute_x  = 0b111,
};

constexpr bool is_alu_or_sta(const opcode op) noexcept
{
    return (static_cast<unsigned>(op) & 0x03u) == 0x01u;
}

/** LDX, LDY, CPX and CPY in any addressing mode. */
constexpr bool is_index_load_or_compare(const opcode op) noexcept
{
    const auto code = static_cast<unsigned>(op);
    const auto mode = (code >> 2) & 0x07u;
    const auto base = code & 0xe3u;

    return (base == 0xa2u or base == 0xa0u or base == 0xe0u or
            base == 0xc0u) and
           (mode == 0b000u or mode == 0b001u or mode == 0b011u);
}

constexpr bool has_modes(const opcode op) noexcept
{
    return is_alu_or_sta(op) or is_index_load_or_compare(op);
}

constexpr opcode with_mode(const opcode op, const addressing mode) noexcept
{
    const auto bits = not is_alu_or_sta(op) and mode == addressing::immediate
                        ? 0u
                        : static_cast<unsigned>(mode);

    return static_cast<opcode>((static_cast<unsigned>(op) & 0xe3u) |
                               (bits << 2));
}

constexpr addressing mode_of(const opcode op) noexcept
{
    const auto bits = (static_cast<unsigned>(op) >> 2) & 0x07u;
    return not is_alu_or_sta(op) and bits == 0 ? addressing::immediate
                                               : static_cast<addressing>(bits);
}

static_assert(with_mode(opcode::lda_imm, addressing::absolute_x) ==
              static_cast<opcode>(0xbd));
static_assert(with_mode(opcode::sta_abs, addressing::absolute_y) ==
              static_cast<opcode>(0x99));
static_assert(with_mode(opcode::sta_abs, addressing::zero_page_x) ==
              static_cast<opcode>(0x95));
static_assert(with_mode(opcode::ldx_imm, addressing::absolute) ==
              static_cast<opcode>(0xae));
static_assert(with_mode(opcode::cpy_imm, addressing::zero_page) ==
              static_cast<opcode>(0xc4));
static_assert(with_mode(with_mode(opcode::ldy_imm, addressing::absolute),
                        addressing::immediate) == opcode::ldy_imm);
static_assert(not has_modes(opcode::inx) and not has_modes(opcode::tsx) and
              has_modes(with_mode(opcode::cpx_imm, addressing::absolute)));

constexpr bool is_store(const opcode op) noexcept
{
    return is_alu_or_sta(op) and (static_cast<unsigned>(op) >> 5) == 0b100u;
}

constexpr bool is_compare(const opcode op) noexcept
{
    const auto base = static_cast<unsigned>(op) & 0xe3u;
    return (is_alu_or_sta(op) and base == 0xc1u) or
           (is_index_load_or_compare(op) and (base == 0xe0u or base == 0xc0u));
}

/** The operations of the ALU and CMP, the bits 7..5 of the opcodes select
 * them. */
constexpr std::array immediate_opcodes{ opcode::ora_imm, opcode::and_imm,
                                        opcode::eor_imm, opcode::adc_imm,
                                        opcode::lda_imm, opcode::cmp_imm,
                                        opcode::sbc_imm };

constexpr std::array index_opcodes{ opcode::inx, opcode::iny, opcode::dex,
                                    opcode::dey, opcode::tsx };

constexpr std::array index_load_and_compare_opcodes{
    opcode::ldx_imm, opcode::ldy_imm, opcode::cpx_imm, opcode::cpy_imm
};

constexpr std::array shift_opcodes{ opcode::asl_a, opcode::rol_a, opcode::lsr_a,
                                    opcode::ror_a };

constexpr bool is_branch(const opcode op) noexcept
{
    return op == opcode::beq or op == opcode::bne;
}

constexpr bool has_absolute_address(const addressing mode) noexcept
{
    return mode == addressing::absolute or mode == addressing::absolute_x or
           mode == addressing::absolute_y;
}

/** The size in bytes of an instruction. */
constexpr int instruction_size(const opcode op) noexcept
{
    if (has_modes(op))
        return has_absolute_address(mode_of(op)) ? 3 : 2;

    switch (op) {
    case opcode::jmp_abs:
    case opcode::jsr_abs:
        return 3;
    case opcode::beq:
    case opcode::bne:
        return 2;
    default:
        return 1;
    }
}

/** The registers of the 6502 seen from the outputs of the CPU. */
struct cpu_state {
    unsigned pc = 0, a = 0, x = 0, y = 0, s = 0, c = 0, v = 0, n = 0, z = 0;
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
    const auto next    = (s.pc + static_cast<unsigned>(instruction_size(op))) &
                      0xffffu;

    const auto set_nz = [&](const unsigned value) {
        s.n = value >> 7;
        s.z = value == 0 ? 1u : 0u;
        return value;
    };

    const auto compare = [&](const unsigned reg, const unsigned operand) {
        s.c = reg >= operand ? 1u : 0u;
        set_nz((reg - operand) & 0xffu);
    };

    const auto push = [&](const unsigned value) {
        mem[0x100u + s.s] = static_cast<irt::u8>(value);
        s.s               = (s.s - 1u) & 0xffu;
    };

    const auto pull = [&]() -> unsigned {
        s.s = (s.s + 1u) & 0xffu;
        return mem[0x100u + s.s];
    };

    if (has_modes(op)) {
        const auto mode = mode_of(op);
        const auto base = static_cast<unsigned>(op) & 0xe3u;
        const auto ea   = [&]() -> unsigned {
            switch (mode) {
            case addressing::zero_page:
                return byte(1);
            case addressing::zero_page_x:
                return (byte(1) + s.x) & 0xffu;
            case addressing::absolute_x:
                return (address + s.x) & 0xffffu;
            case addressing::absolute_y:
                return (address + s.y) & 0xffffu;
            default:
                return address;
            }
        }();
        const auto crossed = ((ea ^ address) & 0xff00u) != 0;
        const auto operand = mode == addressing::immediate
                               ? byte(1)
                               : unsigned{ mem[ea] };
        s.pc               = next;

        const auto indexed = mode == addressing::absolute_x or
                             mode == addressing::absolute_y;
        const auto cycles = mode == addressing::immediate     ? 2
                            : mode == addressing::zero_page   ? 3
                            : mode == addressing::zero_page_x ? 4
                            : indexed and crossed             ? 5
                                                              : 4;

        if (is_store(op)) {
            mem[ea] = static_cast<irt::u8>(s.a);
            return indexed ? 5 : cycles;
        }

        switch (base) {
        case 0xc1u:
            compare(s.a, operand);
            return cycles;
        case 0xa2u:
            s.x = set_nz(operand);
            return cycles;
        case 0xa0u:
            s.y = set_nz(operand);
            return cycles;
        case 0xe0u:
            compare(s.x, operand);
            return cycles;
        case 0xc0u:
            compare(s.y, operand);
            return cycles;
        default:
            break;
        }

        constexpr std::array<alu_op, 8> ops{ alu_op::ora,    alu_op::and_,
                                             alu_op::eor,    alu_op::adc,
                                             alu_op::pass_b, alu_op::pass_b,
                                             alu_op::pass_b, alu_op::sbc };
        const auto alu = ops[static_cast<unsigned>(op) >> 5];
        const auto res = alu_reference(alu, s.a, operand, s.c);
        s.a            = set_nz(res.r);
        if (alu == alu_op::adc or alu == alu_op::sbc) {
            s.c = res.c;
            s.v = res.v;
        }

        return cycles;
    }

    switch (op) {
    case opcode::jmp_abs:
        s.pc = address;
        return 3;

    case opcode::jsr_abs:
        push((s.pc + 2u) >> 8);
        push((s.pc + 2u) & 0xffu);
        s.pc = address;
        return 6;

    case opcode::rts: {
        const auto low = pull();
        s.pc           = ((low | (pull() << 8)) + 1u) & 0xffffu;
        return 6;
    }

    case opcode::pha:
        push(s.a);
        s.pc = next;
        return 3;

    case opcode::pla:
        s.a  = set_nz(pull());
        s.pc = next;
        return 4;

    case opcode::beq:
    case opcode::bne: {
        const auto offset = static_cast<int>(static_cast<irt::i8>(byte(1)));
        s.pc              = next;
        if ((op == opcode::beq) != (s.z == 1))
            return 2;

        const auto target = static_cast<unsigned>(static_cast<int>(next) +
                                                  offset) &
                            0xffffu;
        s.pc = target;
        return ((target ^ next) & 0xff00u) != 0 ? 4 : 3;
    }

    case opcode::asl_a:
    case opcode::rol_a:
    case opcode::lsr_a:
    case opcode::ror_a: {
        const auto left   = op == opcode::asl_a or op == opcode::rol_a;
        const auto rotate = op == opcode::rol_a or op == opcode::ror_a;
        const auto res = alu_reference(left ? alu_op::rol : alu_op::ror, s.a, 0,
                                       rotate ? s.c : 0u);
        s.a            = set_nz(res.r);
        s.c            = res.c;
        break;
    }

    case opcode::inx:
        s.x = set_nz((s.x + 1) & 0xffu);
        break;
    case opcode::iny:
        s.y = set_nz((s.y + 1) & 0xffu);
        break;
    case opcode::dex:
        s.x = set_nz((s.x - 1) & 0xffu);
        break;
    case opcode::dey:
        s.y = set_nz((s.y - 1) & 0xffu);
        break;
    case opcode::txs:
        s.s = s.x;
        break;
    case opcode::tsx:
        s.x = set_nz(s.s);
        break;
    default:
        break;
    }

    s.pc = next;
    return 2;
}

/** The position of sync in the outputs of the CPU. */
constexpr int cpu_sync = 52;

/** The outputs of the CPU: pc0..pc15, a0..a7, x0..x7, y0..y7, s0..s7, c, v,
 * n, z then sync. */
constexpr irt::u64 cpu_outputs(const cpu_state& s, const unsigned sync) noexcept
{
    return s.pc | (irt::u64{ s.a } << 16) | (irt::u64{ s.x } << 24) |
           (irt::u64{ s.y } << 32) | (irt::u64{ s.s } << 40) |
           (irt::u64{ s.c } << 48) | (irt::u64{ s.v } << 49) |
           (irt::u64{ s.n } << 50) | (irt::u64{ s.z } << 51) |
           (irt::u64{ sync } << cpu_sync);
}

/** The components of the CPU: the 8 bits adder compares a register to the
 * operand. */
struct cpu_parts {
    circuit pc, memory, accumulator, flags, index, stack, reg, adder,
      comparator;
};

/** A 6502 reduced to the opcodes above, with the cycles of the 6502: one
 * memory access per cycle. The program starts at address 0, the stack is the
 * page 0x0100 under S. A one-hot sequencer of flip-flops t1 to t5 follows the
 * fetch cycle t0, when none of them is true:
 * - t0: ir takes the opcode, pc + 1;
 * - t1: the immediate or implied operation on A, X, Y or S executes, or a
 *   comparison, or adl takes the low byte of the address or the offset of a
 *   branch; pc + 1 except for the implied operations;
 * - t2: a jump or a taken branch loads pc, or adh takes the high byte of the
 *   address and pc + 1, or the operation reads its operand at the zero page
 *   address or STA writes A, or PHA pushes A, or PLA and RTS increment S;
 * - t3: the operation reads its operand at the address or STA writes A, or
 *   the address plus an index crosses a page, or the taken branch crosses a
 *   page, or PLA pulls A, or JSR pushes the high byte of pc, or RTS pulls
 *   adl;
 * - t4: the operation reads its operand at the address plus an index or STA
 *   writes A, or JSR pushes the low byte of pc, or RTS pulls the high byte of
 *   pc;
 * - t5: JSR reads the high byte of the address and jumps, or RTS adds one to
 *   pc.
 *
 * The 16 bits adder computes the target of a branch, pc plus the offset, or
 * the address plus X or Y. The 8 bits adder compares A, X or Y to the operand.
 * The decoder only recognizes the opcodes above. Outputs: pc0..pc15, a0..a7,
 * x0..x7, y0..y7, s0..s7, c, v, n, z then sync, true in t0. */
circuit make_cpu(irt::component_access& ids, const cpu_parts& parts) noexcept
{
    const auto& [pc, memory, accumulator, flags, index, stack, reg, adder,
                 comparator] = parts;

    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(0, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(53, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.clk   = c.get_or_add_x("clk");

    netlist    n(gen);
    const auto clk = netlist::input(*ret.clk);
    const auto p   = n.child(pc.id);
    const auto mem = n.child(memory.id);
    const auto acc = n.child(accumulator.id);
    const auto f   = n.child(flags.id);
    const auto idx = n.child(index.id);
    const auto sp  = n.child(stack.id);
    const auto ir  = n.child(reg.id);
    const auto adl = n.child(reg.id);
    const auto adh = n.child(reg.id);
    const auto add = n.child(adder.id);
    const auto cmp = n.child(comparator.id);

    const auto bit = [](const irt::child_id child, const circuit& of,
                        const int i) {
        return netlist::output_of(child, of.y[i]);
    };

    const auto link_clk = [&](const irt::child_id child, const circuit& of) {
        n.link(clk, child, compo(*of.clk));
    };

    // The decoder. The operations of the ALU, CMP and STA have the bits 1..0
    // = 01 and the addressing mode in the bits 4..2: among the modes run, bit
    // 2 without bit 3 tells the zero page, bit 3 with bit 2 or 4 an absolute
    // address, and bit 4 adds X with bit 2 or Y without. The other opcodes
    // are recognized on their eight bits.
    std::array<wire, 8> ir_bits, ir_inverse;
    for (int i = 0; i < 8; ++i) {
        ir_bits[i]    = bit(ir, reg, i);
        ir_inverse[i] = n.not_(ir_bits[i]);
    }

    const auto match = [&](const opcode op) {
        std::array<wire, 8> literals;
        for (int i = 0; i < 8; ++i)
            literals[i] = (static_cast<unsigned>(op) >> i) & 1u ? ir_bits[i]
                                                                : ir_inverse[i];
        return n.all(literals);
    };

    const auto ir2 = ir_bits[2];
    const auto ir3 = ir_bits[3];
    const auto ir4 = ir_bits[4];
    const auto ir5 = ir_bits[5];
    const auto ir6 = ir_bits[6];
    const auto ir7 = ir_bits[7];

    const auto group = n.and_(ir_bits[0], ir_inverse[1]);
    const auto store = n.all(std::array{ ir7, ir_inverse[6], ir_inverse[5] });
    const auto cmp_a = n.and_(group,
                              n.all(std::array{ ir7, ir6, ir_inverse[5] }));
    const auto sta   = n.and_(group, store);
    const auto alu   = n.and_(group, n.not_(n.or_(store, cmp_a)));
    const auto carry = n.and_(alu, n.and_(ir6, ir5));

    const auto group_zero_page  = n.and_(group, n.and_(ir2, ir_inverse[3]));
    const auto group_absolute   = n.and_(group, n.and_(ir3, n.or_(ir2, ir4)));
    const auto zero_page_x      = n.and_(group_zero_page, ir4);
    const auto index_x          = n.and_(group, n.and_(ir4, ir2));
    const auto index_y          = n.and_(group, n.and_(ir4, ir_inverse[2]));
    const auto indexed_absolute = n.and_(group_absolute, ir4);

    // LDX, LDY, CPX and CPY: the immediate, zero page then absolute modes.
    constexpr std::array               index_modes{ addressing::immediate,
                                      addressing::zero_page,
                                      addressing::absolute };
    std::array<wire, 4>                index_instructions;
    std::array<std::array<wire, 4>, 3> by_mode;
    for (int i = 0; i < 4; ++i) {
        std::array<wire, 3> modes;
        for (int m = 0; m < 3; ++m)
            by_mode[m][i] = modes[m] = match(
              with_mode(index_load_and_compare_opcodes[i], index_modes[m]));
        index_instructions[i] = n.any(modes);
    }
    const auto& [ldx, ldy, cpx, cpy] = index_instructions;

    const auto zero_page = n.or_(group_zero_page, n.any(by_mode[1]));
    const auto absolute  = n.or_(group_absolute, n.any(by_mode[2]));
    const auto addressed = n.or_(zero_page, absolute);
    const auto compare   = n.any(std::array{ cmp_a, cpx, cpy });

    std::array<wire, 5> implied_index_ops;
    for (int i = 0; i < 5; ++i)
        implied_index_ops[i] = match(index_opcodes[i]);
    const auto& [inx, iny, dex, dey, tsx] = implied_index_ops;
    const auto implied_index              = n.any(implied_index_ops);
    const auto index_load                 = n.or_(ldx, ldy);
    const auto index_op                   = n.or_(implied_index, index_load);

    std::array<wire, 4> shifts;
    for (int i = 0; i < 4; ++i)
        shifts[i] = match(shift_opcodes[i]);
    const auto& [asl, rol, lsr, ror] = shifts;
    const auto shift                 = n.any(shifts);
    const auto shift_left            = n.or_(asl, rol);
    const auto shift_in_zero         = n.or_(asl, lsr);

    const auto jmp      = match(opcode::jmp_abs);
    const auto beq      = match(opcode::beq);
    const auto bne      = match(opcode::bne);
    const auto branch   = n.or_(beq, bne);
    const auto pha      = match(opcode::pha);
    const auto pla      = match(opcode::pla);
    const auto jsr      = match(opcode::jsr_abs);
    const auto rts      = match(opcode::rts);
    const auto txs      = match(opcode::txs);
    const auto stack_op = n.any(std::array{ pha, pla, jsr, rts });
    const auto implied  = n.any(
      std::array{ implied_index, txs, pha, pla, rts, shift });

    const auto z     = bit(f, flags, 3);
    const auto taken = n.or_(n.and_(beq, z), n.and_(bne, n.not_(z)));

    // The adder: a0..a15, b0..b15 then cin, left false. A branch adds the
    // offset in adl, extended to 16 bits, to pc. Otherwise X, Y or zero is
    // added to the address in adh and adl.
    const auto          no_branch = n.not_(branch);
    std::array<wire, 8> high;
    for (int i = 0; i < 16; ++i) {
        const auto address = i < 8 ? bit(adl, reg, i) : bit(adh, reg, i - 8);
        const auto a       = n.or_(n.and_(branch, bit(p, pc, i)),
                                   n.and_(no_branch, address));
        const auto b       = i < 8 ? n.any(std::array{
                                 n.and_(branch, bit(adl, reg, i)),
                                 n.and_(index_x, bit(idx, index, i)),
                                 n.and_(index_y, bit(idx, index, 8 + i)) })
                                   : n.and_(branch, bit(adl, reg, 7));
        n.link(a, add, compo(adder.x[i]));
        n.link(b, add, compo(adder.x[16 + i]));
        if (i >= 8)
            high[i - 8] = a;
    }

    std::array<wire, 8> page_changes;
    for (int i = 0; i < 8; ++i)
        page_changes[i] = n.xor_(bit(add, adder, 8 + i), high[i]);
    const auto page_crossed = n.any(page_changes);

    // The sequencer. The operand is read in t2 at a zero page address, in t3
    // at a zero page address plus X or at an absolute address, in t4 at an
    // absolute address plus an index in the next page.
    std::array<wire, 5> t;
    for (auto& ff : t)
        ff = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto t0 = n.not_(n.any(t));

    const auto one_more = n.and_(indexed_absolute, n.or_(sta, page_crossed));
    const auto last     = n.and_(
      addressed,
      n.any(std::array{ n.and_(t[1], n.and_(zero_page, n.not_(zero_page_x))),
                        n.and_(t[2], n.not_(one_more)), t[3] }));
    const auto subroutine = n.or_(jsr, rts);

    const std::array<wire, 5> next{
        t0, n.and_(t[0], n.any(std::array{ addressed, jmp, taken, stack_op })),
        n.and_(t[1], n.any(std::array{ absolute, zero_page_x,
                                       n.and_(branch, page_crossed), pla,
                                       subroutine })),
        n.and_(t[2], n.or_(one_more, subroutine)), n.and_(t[3], subroutine)
    };
    for (int i = 0; i < 5; ++i) {
        n.link(next[i], *t[i].child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *t[i].child, mdl(irt::logical_d_flipflop::port_clk));
    }

    // The operand of an operation is read in t1 without address, at the end
    // of the address otherwise.
    const auto operand  = n.or_(n.and_(t[0], n.not_(addressed)), last);
    const auto execute  = n.and_(operand, alu);
    const auto shifted  = n.and_(t[0], shift);
    const auto pull_a   = n.and_(t[2], pla);
    const auto compared = n.and_(operand, compare);
    const auto push_pc  = n.and_(jsr, n.or_(t[2], t[3]));

    const auto load_adl = n.or_(
      n.and_(t[0], n.any(std::array{ addressed, jmp, branch, jsr })),
      n.and_(t[2], rts));
    const auto load_adh = n.and_(t[1], absolute);
    const auto jump     = n.any(std::array{ n.and_(t[1], n.or_(jmp, branch)),
                                        n.and_(t[4], jsr), n.and_(t[3], rts) });
    const auto write    = n.any(
      std::array{ n.and_(last, sta), n.and_(t[1], pha), push_pc });

    // The registers: d0..d7 then load.
    const std::array registers{ std::pair{ ir, t0 }, std::pair{ adl, load_adl },
                                std::pair{ adh, load_adh } };
    for (const auto& [r, load] : registers) {
        for (int i = 0; i < 8; ++i)
            n.link(bit(mem, memory, i), r, compo(reg.x[i]));
        n.link(load, r, compo(reg.x[8]));
        link_clk(r, reg);
    }

    // The program counter: d0..d15, load then inc. JMP, JSR and RTS take adl
    // and the byte read, a branch the target. pc + 1 in t0, t1 but for the
    // implied operations, t2 but for the stack and the zero page, and t5: a
    // load wins.
    const auto from_memory = n.any(std::array{ jmp, jsr, rts });
    const auto from_adder  = n.not_(from_memory);
    for (int i = 0; i < 16; ++i) {
        const auto address = i < 8 ? bit(adl, reg, i) : bit(mem, memory, i - 8);
        n.link(n.or_(n.and_(from_memory, address),
                     n.and_(from_adder, bit(add, adder, i))),
               p, compo(pc.x[i]));
    }
    n.link(jump, p, compo(pc.x[16]));
    n.link(
      n.not_(n.any(std::array{ t[2], t[3], n.and_(t[0], implied),
                               n.and_(t[1], n.or_(stack_op, zero_page)) })),
      p, compo(pc.x[17]));
    link_clk(p, pc);

    // The accumulator: b0..b7, cin, op0..op2 then load. The operation is the
    // bits 7..5 of the opcode, except LDA (101) and PLA which become pass_b
    // (111), SBC (111) which becomes sbc (100), ASL and ROL which become rol
    // (101) and LSR and ROR which become ror (110). ASL and LSR shift in a
    // zero.
    for (int i = 0; i < 8; ++i)
        n.link(bit(mem, memory, i), acc, compo(accumulator.x[i]));
    n.link(n.and_(bit(f, flags, 0), n.not_(shift_in_zero)), acc,
           compo(accumulator.x[8]));
    n.link(n.or_(n.and_(n.not_(shift), n.and_(ir5, n.not_(n.and_(ir7, ir6)))),
                 shift_left),
           acc, compo(accumulator.x[9]));
    n.link(n.xor_(ir7, ir6), acc, compo(accumulator.x[10]));
    n.link(n.any(std::array{ ir7, pla, shift }), acc, compo(accumulator.x[11]));
    n.link(n.any(std::array{ execute, pull_a, shifted }), acc,
           compo(accumulator.x[12]));
    link_clk(acc, accumulator);

    // The index registers: d0..d7, imm, dec, sel_y then write.
    const auto index_execute = n.or_(n.and_(t[0], implied_index),
                                     n.and_(operand, index_load));
    const auto from_bus      = n.not_(tsx);
    for (int i = 0; i < 8; ++i)
        n.link(n.or_(n.and_(tsx, bit(sp, stack, i)),
                     n.and_(from_bus, bit(mem, memory, i))),
               idx, compo(index.x[i]));
    n.link(n.or_(index_load, tsx), idx, compo(index.x[8]));
    n.link(n.or_(dex, dey), idx, compo(index.x[9]));
    n.link(n.any(std::array{ ldy, iny, dey }), idx, compo(index.x[10]));
    n.link(index_execute, idx, compo(index.x[11]));
    link_clk(idx, index);

    // The stack pointer: d0..d7, load, inc then dec.
    for (int i = 0; i < 8; ++i)
        n.link(bit(idx, index, i), sp, compo(stack.x[i]));
    n.link(n.and_(t[0], txs), sp, compo(stack.x[8]));
    n.link(n.or_(n.and_(t[1], pla), n.and_(rts, n.or_(t[1], t[2]))), sp,
           compo(stack.x[9]));
    n.link(n.or_(n.and_(t[1], pha), push_pc), sp, compo(stack.x[10]));
    link_clk(sp, stack);

    // The comparator: a0..a7, b0..b7 then cin. The register minus the
    // operand is the register plus the complement of the operand plus one.
    for (int i = 0; i < 8; ++i) {
        n.link(n.any(std::array{ n.and_(cmp_a, bit(acc, accumulator, i)),
                                 n.and_(cpx, bit(idx, index, i)),
                                 n.and_(cpy, bit(idx, index, 8 + i)) }),
               cmp, compo(comparator.x[i]));
        n.link(n.not_(bit(mem, memory, i)), cmp, compo(comparator.x[8 + i]));
    }
    n.link(compare, cmp, compo(comparator.x[16]));

    std::array<wire, 8> difference;
    for (int i = 0; i < 8; ++i)
        difference[i] = bit(cmp, comparator, i);

    // The flags: c, v, n, z, load_c, load_v then load_nz. N and Z come from
    // the index registers, the comparator or the ALU.
    const auto from_alu = n.not_(n.or_(index_op, compare));
    n.link(n.or_(n.and_(compare, bit(cmp, comparator, 8)),
                 n.and_(n.not_(compare), bit(acc, accumulator, 8))),
           f, compo(flags.x[0]));
    n.link(bit(acc, accumulator, 9), f, compo(flags.x[1]));
    const std::array<wire, 2> nz{ difference[7], n.not_(n.any(difference)) };
    for (int i = 0; i < 2; ++i)
        n.link(
          n.any(std::array{ n.and_(index_op, bit(idx, index, 16 + i)),
                            n.and_(compare, nz[i]),
                            n.and_(from_alu, bit(acc, accumulator, 10 + i)) }),
          f, compo(flags.x[2 + i]));
    const auto load_cv = n.and_(execute, carry);
    n.link(n.any(std::array{ load_cv, compared, shifted }), f,
           compo(flags.x[4]));
    n.link(load_cv, f, compo(flags.x[5]));
    n.link(
      n.any(std::array{ execute, pull_a, index_execute, compared, shifted }), f,
      compo(flags.x[6]));
    link_clk(f, flags);

    // The memory: a0..a15, d0..d7 then we. The address is pc, the sum of the
    // adder at the end of an address, without its high byte in the zero page,
    // or 0x0100 plus S in the stack. JSR pushes the high byte of pc then the
    // low byte, the others A.
    const auto in_stack  = n.and_(stack_op,
                                  n.any(std::array{ t[1], t[2], t[3] }));
    const auto effective = n.and_(
      addressed, n.any(std::array{ n.and_(t[1], zero_page), t[2], t[3] }));
    const auto effective_high = n.and_(effective, n.not_(zero_page));
    const auto fetch          = n.not_(n.or_(in_stack, effective));
    for (int i = 0; i < 16; ++i) {
        auto address = n.or_(
          n.and_(i < 8 ? effective : effective_high, bit(add, adder, i)),
          n.and_(fetch, bit(p, pc, i)));
        if (i < 8)
            address = n.or_(n.and_(in_stack, bit(sp, stack, i)), address);
        else if (i == 8)
            address = n.or_(in_stack, address);

        n.link(address, mem, compo(memory.x[i]));
    }

    const auto push_high = n.and_(jsr, t[2]);
    const auto push_low  = n.and_(jsr, t[3]);
    const auto push_a    = n.not_(jsr);
    for (int i = 0; i < 8; ++i)
        n.link(n.any(std::array{ n.and_(push_a, bit(acc, accumulator, i)),
                                 n.and_(push_high, bit(p, pc, 8 + i)),
                                 n.and_(push_low, bit(p, pc, i)) }),
               mem, compo(memory.x[16 + i]));
    n.link(write, mem, compo(memory.x[24]));
    link_clk(mem, memory);

    const auto output = [&](const std::string_view name, const wire& w) {
        n.output(*ret.y.emplace_back(c.get_or_add_y(name)), w);
    };

    for (int i = 0; i < 16; ++i)
        output(irt::format_n<8>("pc{}", i).sv(), bit(p, pc, i));
    for (int i = 0; i < 8; ++i)
        output(irt::format_n<8>("a{}", i).sv(), bit(acc, accumulator, i));
    for (int i = 0; i < 16; ++i)
        output(irt::format_n<8>("{}{}", i < 8 ? 'x' : 'y', i % 8).sv(),
               bit(idx, index, i));
    for (int i = 0; i < 8; ++i)
        output(irt::format_n<8>("s{}", i).sv(), bit(sp, stack, i));

    constexpr std::array<const char*, 4> names{ "c", "v", "n", "z" };
    for (int i = 0; i < 4; ++i)
        output(names[i], bit(f, flags, i));
    output("sync", t0);

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
                const auto* f = fs.file_paths.try_to_get(
                  ids.component_file_paths[id].file);
                boost::ut::expect(
                  irt::is_loaded(ids.components[id].file_status))
                  << (f ? f->path.sv() : std::string_view("?"));

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

cpu_parts make_cpu_parts(irt::component_access&   ids,
                         std::span<const irt::u8> program) noexcept
{
    const auto fa      = make_full_adder(ids);
    const auto adder_8 = make_adder<8>(ids, fa);
    const auto alu     = make_alu(ids, adder_8);
    const auto reg     = make_register<8>(ids);

    return cpu_parts{ .pc          = make_program_counter(ids),
                      .memory      = make_memory(ids, program),
                      .accumulator = make_accumulator(ids, alu, reg),
                      .flags       = make_flags(ids),
                      .index       = make_index_registers(ids, reg),
                      .stack       = make_stack_pointer(ids, reg),
                      .reg         = reg,
                      .adder       = as_circuit(make_adder<16>(ids, fa)),
                      .comparator  = as_circuit(adder_8) };
}

bench make_cpu_bench(irt::component_access&   ids,
                     std::span<const irt::u8> program) noexcept
{
    return make_bench(ids, make_cpu(ids, make_cpu_parts(ids, program)));
}

/** The branches taken during a run, some of them cross a page. */
struct cpu_run {
    int taken_branches = 0;
    int crossed_pages  = 0;

    /** The reads at an absolute address plus X or Y in the next page. */
    int indexed_crossings = 0, y_crossings = 0;

    /** The zero page addresses plus X which wrap in the zero page. */
    int zero_page_wraps = 0;

    int calls = 0;

    /** The comparisons of a register greater, equal or lower than the
     * operand. */
    int compares_greater = 0, compares_equal = 0, compares_lower = 0;

    /** The lowest address written by a push, in the page 0x0100. */
    unsigned lowest_push = 0xff;
};

/** Runs @a instructions instructions of the @a program on the CPU and on the
 * reference, cycle by cycle: sync is true only on the first cycle of an
 * instruction, when the registers show the results of the previous ones. */
cpu_run check_cpu(bench_simulation&        sim,
                  std::span<const irt::u8> program,
                  const int                instructions) noexcept
{
    cpu_run              run;
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
        const auto op       = static_cast<opcode>(bytes[pc]);
        const auto mode  = has_modes(op) ? mode_of(op) : addressing::immediate;
        const auto wraps = bytes[(pc + 1) & 0xffffu] + ref.x > 0xffu;
        const auto duration = cpu_reference(ref, bytes);

        if (is_branch(op) and duration > 2)
            ++run.taken_branches;
        if (is_branch(op) and duration > 3)
            ++run.crossed_pages;
        if ((mode == addressing::absolute_x or
             mode == addressing::absolute_y) and
            not is_store(op) and duration > 4) {
            ++run.indexed_crossings;
            if (mode == addressing::absolute_y)
                ++run.y_crossings;
        }
        if (mode == addressing::zero_page_x and wraps)
            ++run.zero_page_wraps;
        if (op == opcode::jsr_abs)
            ++run.calls;
        if (op == opcode::pha or op == opcode::jsr_abs)
            run.lowest_push = std::min(run.lowest_push, (ref.s + 1u) & 0xffu);
        if (is_compare(op)) {
            if (ref.z)
                ++run.compares_equal;
            else if (ref.c)
                ++run.compares_greater;
            else
                ++run.compares_lower;
        }

        for (int j = 1; j <= duration; ++j) {
            const auto out = sim.cycle();
            ++k;

            if (j < duration)
                boost::ut::expect(boost::ut::eq((out >> cpu_sync) & 1u, 0u))
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

    return run;
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

    "stack-pointer-loads-increments-and-decrements"_test = [] {
        irt::modeling mod;

        const auto b = mod.ids.write([&](auto& ids) {
            return make_bench(ids,
                              make_stack_pointer(ids, make_register<8>(ids)));
        });

        bench_simulation sim(mod, b);
        std::mt19937     gen(6502u);

        // Some loads go near 0x00 and 0xff to check the wraps.
        constexpr int cycles = 500;
        irt::u64      s      = 0;
        int           wraps  = 0;
        sim.start(0, cycles);
        for (int k = 1; k <= cycles; ++k) {
            const irt::u64 d    = gen() % 2 ? (gen() % 2 ? 0x01u : 0xfeu)
                                            : gen() & 0xffu;
            const auto     kind = gen() % 8;
            const irt::u64 load = kind == 0 ? 1u : 0u;
            const irt::u64 inc  = kind > 0 and kind < 4 ? 1u : 0u;
            const irt::u64 dec  = kind >= 4 and kind < 7 ? 1u : 0u;

            sim.set_inputs(d | (load << 8) | (inc << 9) | (dec << 10));
            if ((inc and s == 0xffu) or (dec and s == 0x00u))
                ++wraps;
            s = load ? d : inc ? (s + 1) & 0xffu : dec ? (s - 1) & 0xffu : s;

            expect(eq(sim.cycle(), s)) << "cycle " << k << " load " << load
                                       << " inc " << inc << " dec " << dec;
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

    "cpu-runs-loops-with-the-index-registers"_test = [] {
        irt::vector<irt::u8> program(0x103, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                             std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        put(0x0000, { 0xa2, 0x03 });       // LDX #$03
        put(0x0002, { 0xca });             // DEX
        put(0x0003, { 0xd0, 0xfd });       // BNE $0002, taken twice
        put(0x0005, { 0xa0, 0xfe });       // LDY #$FE
        put(0x0007, { 0xc8, 0xc8 });       // INY, INY: Z
        put(0x0009, { 0xf0, 0x01 });       // BEQ $000C
        put(0x000b, { 0xe8 });             // INX, skipped
        put(0x000c, { 0x88 });             // DEY: N
        put(0x000d, { 0xa9, 0x40 });       // LDA #$40
        put(0x000f, { 0x69, 0x40 });       // ADC #$40
        put(0x0011, { 0x8d, 0x00, 0x80 }); // STA $8000
        put(0x0014, { 0x4c, 0xf8, 0x00 }); // JMP $00F8
        put(0x00f2, { 0x4c, 0x00, 0x00 }); // JMP $0000
        put(0x00f8, { 0xe8 });             // INX
        put(0x00f9, { 0xf0, 0x00 });       // BEQ, not taken
        put(0x00fb, { 0xd0, 0x03 });       // BNE $0100, next page
        put(0x0100, { 0xd0, 0xf0 });       // BNE $00F2, previous page

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 60);
        expect(gt(run.taken_branches, run.crossed_pages));
        expect(gt(run.crossed_pages, 0));
    };

    "cpu-reads-and-writes-with-the-addressing-modes"_test = [] {
        // X = $10: $20F8,X is $2108 in the next page.
        constexpr std::array<irt::u8, 32> program{
            0xa2, 0x10,       // LDX #$10
            0xa9, 0x5a,       // LDA #$5A
            0x9d, 0xf8, 0x20, // STA $20F8,X: $2108, 5 cycles
            0xa9, 0x00,       // LDA #$00
            0xad, 0x08, 0x21, // LDA $2108: $5A
            0x7d, 0xf8, 0x20, // ADC $20F8,X: $5A + $5A, 5 cycles
            0x8d, 0x00, 0x30, // STA $3000
            0xbd, 0xf0, 0x2f, // LDA $2FF0,X: $3000, 5 cycles
            0x5d, 0x00, 0x00, // EOR $0000,X: $0010, 4 cycles
            0x2d, 0x00, 0x00, // AND $0000: $A2
            0x4c, 0x00, 0x00  // JMP $0000
        };

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 33);
        expect(eq(run.indexed_crossings, 6));
    };

    "cpu-calls-subroutines-and-pushes-on-the-stack"_test = [] {
        irt::vector<irt::u8> program(0x1f3, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                             std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // S starts at zero: the first push writes $0100, the INX of the
        // program, and S wraps.
        put(0x0000, { 0xa9, 0xe8 });       // LDA #$E8
        put(0x0002, { 0x48 });             // PHA: $0100, S = $FF
        put(0x0003, { 0xa9, 0x00 });       // LDA #$00: Z
        put(0x0005, { 0x68 });             // PLA: $E8, S = $00
        put(0x0006, { 0xa2, 0xff });       // LDX #$FF
        put(0x0008, { 0x9a });             // TXS
        put(0x0009, { 0xa2, 0x00 });       // LDX #$00
        put(0x000b, { 0xa9, 0x11 });       // LDA #$11
        put(0x000d, { 0x48 });             // PHA
        put(0x000e, { 0x20, 0x40, 0x00 }); // JSR $0040
        put(0x0011, { 0x68 });             // PLA: $11
        put(0x0012, { 0x8d, 0x02, 0x20 }); // STA $2002
        put(0x0015, { 0xba });             // TSX: $FF
        put(0x0016, { 0x4c, 0xed, 0x01 }); // JMP $01ED
        put(0x0040, { 0xa9, 0x22 });       // LDA #$22
        put(0x0042, { 0x48 });             // PHA
        put(0x0043, { 0x20, 0x50, 0x00 }); // JSR $0050, nested
        put(0x0046, { 0x68 });             // PLA: $22
        put(0x0047, { 0x8d, 0x00, 0x20 }); // STA $2000
        put(0x004a, { 0x60 });             // RTS
        put(0x0050, { 0xba });             // TSX: $FA
        put(0x0051, { 0xa9, 0x33 });       // LDA #$33
        put(0x0053, { 0x8d, 0x01, 0x20 }); // STA $2001
        put(0x0056, { 0x60 });             // RTS
        put(0x00fd, { 0x20, 0x00, 0x01 }); // JSR $0100: pushes $00FF
        put(0x0100, { 0xe8 });             // INX
        put(0x0101, { 0x60 });             // RTS: to $0100 then $01F0
        put(0x01ed, { 0x20, 0xfd, 0x00 }); // JSR $00FD: pushes $01EF
        put(0x01f0, { 0x4c, 0x06, 0x00 }); // JMP $0006

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 70);
        expect(gt(run.calls, 4));
    };

    "cpu-compares-the-registers-to-the-operands"_test = [] {
        irt::vector<irt::u8> program(0x33, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                             std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // $40 + $40 sets V and N, V stays through the comparisons.
        put(0x0000, { 0xa9, 0x40 });       // LDA #$40
        put(0x0002, { 0x69, 0x40 });       // ADC #$40: $80
        put(0x0004, { 0xc9, 0x80 });       // CMP #$80: Z, C
        put(0x0006, { 0xc9, 0x81 });       // CMP #$81: N
        put(0x0008, { 0xc9, 0x7f });       // CMP #$7F: C
        put(0x000a, { 0xcd, 0x30, 0x00 }); // CMP $0030: $80, Z, C
        put(0x000d, { 0xa2, 0x05 });       // LDX #$05
        put(0x000f, { 0xdd, 0x2b, 0x00 }); // CMP $002B,X: $80, Z, C
        put(0x0012, { 0xe0, 0x05 });       // CPX #$05: Z, C
        put(0x0014, { 0xe0, 0x06 });       // CPX #$06: N
        put(0x0016, { 0xec, 0x31, 0x00 }); // CPX $0031: $05, Z, C
        put(0x0019, { 0xa0, 0xf0 });       // LDY #$F0
        put(0x001b, { 0xc0, 0x10 });       // CPY #$10: N, C
        put(0x001d, { 0xcc, 0x32, 0x00 }); // CPY $0032: $F1, N
        put(0x0020, { 0xd0, 0xde });       // BNE $0000
        put(0x0030, { 0x80, 0x05, 0xf1 });

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 40);
        expect(gt(run.compares_greater, 0));
        expect(gt(run.compares_equal, 0));
        expect(gt(run.compares_lower, 0));
    };

    "cpu-addresses-the-zero-page-and-indexes-with-y"_test = [] {
        irt::vector<irt::u8> program(0x50, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                             std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // $FC,X wraps to $4C with X = $50, $20F8,Y crosses to $2108. The
        // writes stay above the code.
        put(0x0000, { 0xa2, 0x50 });       // LDX #$50
        put(0x0002, { 0xa0, 0x10 });       // LDY #$10
        put(0x0004, { 0xa9, 0x3c });       // LDA #$3C
        put(0x0006, { 0x85, 0x48 });       // STA $48, 3 cycles
        put(0x0008, { 0x95, 0xfc });       // STA $FC,X: $4C, 4 cycles
        put(0x000a, { 0x99, 0xf8, 0x20 }); // STA $20F8,Y: $2108, 5 cycles
        put(0x000d, { 0xa9, 0x00 });       // LDA #$00
        put(0x000f, { 0x65, 0x48 });       // ADC $48: 3 cycles
        put(0x0011, { 0x75, 0xf8 });       // ADC $F8,X: $48, 4 cycles
        put(0x0013, { 0x79, 0xf8, 0x20 }); // ADC $20F8,Y: 5 cycles
        put(0x0016, { 0xd9, 0x38, 0x00 }); // CMP $0038,Y: $48, 4 cycles
        put(0x0019, { 0xa6, 0x4c });       // LDX $4C: $3C
        put(0x001b, { 0xae, 0x08, 0x21 }); // LDX $2108: $3C
        put(0x001e, { 0xa4, 0x48 });       // LDY $48: $3C
        put(0x0020, { 0xac, 0x4c, 0x00 }); // LDY $004C: $3C
        put(0x0023, { 0xe4, 0x48 });       // CPX $48: Z
        put(0x0025, { 0xc4, 0x40 });       // CPY $40: C
        put(0x0027, { 0x55, 0x04 });       // EOR $04,X: $40
        put(0x0029, { 0x4c, 0x00, 0x00 }); // JMP $0000
        put(0x0040, { 0x07 });

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 40);
        expect(gt(run.y_crossings, 0));
        expect(gt(run.zero_page_wraps, 0));
    };

    "cpu-shifts-and-rotates-the-accumulator"_test = [] {
        // The carry goes out of a bit 7 or 0 and comes back in a rotation.
        constexpr std::array<irt::u8, 16> program{
            0xa9, 0x81,      // LDA #$81
            0x0a,            // ASL: $02, C
            0x2a,            // ROL: $05
            0x4a,            // LSR: $02, C
            0x6a,            // ROR: $81, N
            0x6a,            // ROR: $40, C
            0x4a,            // LSR: $20
            0x2a,            // ROL: $40
            0x0a,            // ASL: $80, N
            0x0a,            // ASL: $00, C, Z
            0x0a,            // ASL: $00, Z
            0x4a,            // LSR: $00, Z
            0x4c, 0x02, 0x00 // JMP $0002
        };

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        check_cpu(sim, program, 60);
    };

    "cpu-runs-random-programs"_test = [] {
        cpu_run total;

        for (const auto seed : { 2024u, 6502u, 1975u, 6510u, 1977u }) {
            std::mt19937 gen(seed);

            constexpr std::array modes{
                addressing::immediate,   addressing::zero_page,
                addressing::zero_page_x, addressing::absolute,
                addressing::absolute_x,  addressing::absolute_y
            };
            constexpr std::array index_modes{ addressing::immediate,
                                              addressing::zero_page,
                                              addressing::absolute };
            constexpr std::array store_modes{ addressing::zero_page,
                                              addressing::absolute,
                                              addressing::absolute_x,
                                              addressing::absolute_y };
            const auto           pick = [&](const auto& opcodes) {
                return opcodes[gen() % opcodes.size()];
            };

            // The instructions without jump, branch nor stack. STA $zp,X
            // could write anywhere in the zero page, over the subroutines.
            const auto plain = [&]() {
                const auto kind = gen() % 16;
                return kind < 7
                         ? with_mode(pick(immediate_opcodes), pick(modes))
                       : kind < 9 ? pick(index_opcodes)
                       : kind < 11
                         ? with_mode(pick(index_load_and_compare_opcodes),
                                     pick(index_modes))
                       : kind < 13
                         ? with_mode(opcode::sta_abs, pick(store_modes))
                         : pick(shift_opcodes);
            };

            // The subroutines follow a JMP $0080, without jump nor branch,
            // and leave the stack as they find it. The zero page writes stay
            // in 0x40..0x7f, above them. The main code lies around the page
            // 0x0100, after a LDX #$C0, TXS: some branches cross the page and
            // the stack, above the code, neither reaches it with the pushes
            // nor wraps with the pulls.
            irt::vector<opcode> ops(200, irt::reserve_tag);
            irt::vector<int>    starts(200, irt::reserve_tag);
            int                 size = 3;
            const auto          add  = [&](const opcode op) {
                ops.emplace_back(op);
                starts.emplace_back(size);
                size += instruction_size(op);
            };

            std::array<int, 4> subroutines;
            for (auto& start : subroutines) {
                start              = size;
                const auto balance = gen() % 2;
                if (balance)
                    add(opcode::pha);
                for (int i = 0, e = 1 + static_cast<int>(gen() % 4); i < e; ++i)
                    add(plain());
                if (balance)
                    add(opcode::pla);
                add(opcode::rts);
            }
            expect(fatal(le(size, 0x40)));

            constexpr int origin     = 0x83;
            const auto    main_begin = ops.ssize();
            size                     = origin;
            while (size < origin + 256) {
                const auto kind = gen() % 20;
                add(kind < 13   ? plain()
                    : kind < 14 ? opcode::jmp_abs
                    : kind < 16 ? (gen() % 2 ? opcode::beq : opcode::bne)
                    : kind < 17 ? opcode::pha
                    : kind < 18 ? opcode::pla
                                : opcode::jsr_abs);
            }
            add(opcode::jmp_abs);

            // The jumps and the branches go to the start of another
            // instruction of the main code, the jumps a little forward: the
            // loops come from the branches and the code runs to its end. The
            // writes stay in a window above the program, around a page: the
            // reads are in this window or in the program.
            const auto main_starts = std::span(starts.data() + main_begin,
                                               starts.data() + starts.ssize());
            const auto target      = [&](const int from, const bool near) {
                irt::vector<int> candidates(starts.ssize(), irt::reserve_tag);
                for (const auto start : main_starts) {
                    const auto offset = start - (from + 2);
                    if (near
                               ? start != from and offset >= -128 and offset <= 127
                               : start > from and start - from <= 48)
                        candidates.emplace_back(start);
                }

                return candidates[static_cast<int>(gen() % candidates.size())];
            };

            irt::vector<irt::u8> program(512, irt::reserve_tag);
            const auto           put = [&](std::initializer_list<int> bytes) {
                for (const auto byte : bytes)
                    program.emplace_back(static_cast<irt::u8>(byte));
            };

            put({ 0x4c, 0x80, 0x00 }); // JMP $0080
            for (int i = 0; i < ops.ssize(); ++i) {
                if (i == main_begin) {
                    while (program.ssize() < 0x80)
                        put({ 0x00 });
                    put({ 0xa2, 0xc0, 0x9a }); // LDX #$C0, TXS
                }

                const auto op = ops[i];
                put({ static_cast<int>(op) });

                const auto mode     = has_modes(op) ? mode_of(op)
                                                    : addressing::immediate;
                const auto absolute = has_modes(op) and
                                      has_absolute_address(mode);
                const auto store = is_store(op);

                if (i + 1 == ops.ssize()) {
                    put({ 0x00, 0x00 }); // JMP $0000
                } else if (absolute or op == opcode::jmp_abs or
                           op == opcode::jsr_abs) {
                    const auto address = op == opcode::jmp_abs
                                           ? static_cast<unsigned>(
                                               target(starts[i], false))
                                         : op == opcode::jsr_abs
                                           ? static_cast<unsigned>(
                                               pick(subroutines))
                                         : store or gen() % 2
                                           ? 0x10c0u + (gen() & 0x7fu)
                                           : gen() & 0x1ffu;
                    put({ static_cast<int>(address & 0xffu),
                          static_cast<int>(address >> 8) });
                } else if (is_branch(op)) {
                    put({ (target(starts[i], true) - (starts[i] + 2)) & 0xff });
                } else if (store) {
                    put({ static_cast<int>(0x40u + (gen() & 0x3fu)) });
                } else if (instruction_size(op) == 2) {
                    put({ static_cast<int>(gen() & 0xffu) });
                }
            }
            expect(fatal(eq(program.ssize(), size)));

            irt::modeling mod;
            const auto    b = mod.ids.write(
              [&](auto& ids) { return make_cpu_bench(ids, program); });

            bench_simulation sim(mod, b);
            const auto       run = check_cpu(sim, program, 300);
            expect(gt(0x100 + static_cast<int>(run.lowest_push), size));

            total.taken_branches += run.taken_branches;
            total.crossed_pages += run.crossed_pages;
            total.indexed_crossings += run.indexed_crossings;
            total.y_crossings += run.y_crossings;
            total.zero_page_wraps += run.zero_page_wraps;
            total.calls += run.calls;
            total.compares_greater += run.compares_greater;
            total.compares_equal += run.compares_equal;
            total.compares_lower += run.compares_lower;
        }

        expect(gt(total.taken_branches, total.crossed_pages));
        expect(gt(total.crossed_pages, 0));
        expect(gt(total.indexed_crossings, total.y_crossings));
        expect(gt(total.y_crossings, 0));
        expect(gt(total.zero_page_wraps, 0));
        expect(gt(total.calls, 0));
        expect(gt(total.compares_greater, 0));
        expect(gt(total.compares_equal, 0));
        expect(gt(total.compares_lower, 0));
    };

    "cpu-survives-a-round-trip-through-irt-files"_test = [] {
        const auto* keep = std::getenv("IRRITATOR_CIRCUITS_DIR");
        const auto  temp = irt::temp_path_with_unlink{ "cpu" };
        expect(fatal(temp.success()));
        const std::string_view dir = keep ? keep : temp.c_str();

        // X counts down from 5 and the subroutine adds 3 to A each time: A =
        // 15.
        constexpr std::array<irt::u8, 17> program{
            0xa2, 0x05,       // LDX #$05
            0x20, 0x0e, 0x00, // JSR $000E
            0xca,             // DEX
            0xd0, 0xfa,       // BNE $0002
            0x8d, 0x00, 0x20, // STA $2000
            0x4c, 0x0b, 0x00, // JMP $000B
            0x69, 0x03,       // ADC #$03
            0x60              // RTS
        };
        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto      fa      = make_full_adder(ids);
                const auto      adder_8 = make_adder<8>(ids, fa);
                const auto      alu     = make_alu(ids, adder_8);
                const auto      reg     = make_register<8>(ids);
                const cpu_parts parts{
                    .pc          = make_program_counter(ids),
                    .memory      = make_memory(ids, program),
                    .accumulator = make_accumulator(ids, alu, reg),
                    .flags       = make_flags(ids),
                    .index       = make_index_registers(ids, reg),
                    .stack       = make_stack_pointer(ids, reg),
                    .reg         = reg,
                    .adder       = as_circuit(make_adder<16>(ids, fa)),
                    .comparator  = as_circuit(adder_8)
                };
                const auto cpu = make_cpu(ids, parts);
                const auto b   = make_bench(ids, cpu);
                return std::array{ fa.id,
                                   adder_8.id,
                                   parts.adder.id,
                                   alu.id,
                                   reg.id,
                                   parts.accumulator.id,
                                   parts.flags.id,
                                   parts.index.id,
                                   parts.stack.id,
                                   parts.pc.id,
                                   parts.memory.id,
                                   cpu.id,
                                   b.id };
            });

            save_circuits(mod, dir,
                          { { compo[0], "full-adder.irt" },
                            { compo[1], "adder-8.irt" },
                            { compo[2], "adder-16.irt" },
                            { compo[3], "alu-8.irt" },
                            { compo[4], "register-8.irt" },
                            { compo[5], "accumulator.irt" },
                            { compo[6], "flags.irt" },
                            { compo[7], "index-registers.irt" },
                            { compo[8], "stack-pointer.irt" },
                            { compo[9], "program-counter.irt" },
                            { compo[10], "cpu-memory.irt" },
                            { compo[11], "cpu.irt" },
                            { compo[12], "cpu-bench.irt" } });
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "cpu-bench.irt", 0, 53);
        bench_simulation sim(mod, b);
        check_cpu(sim, program, 30);
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
