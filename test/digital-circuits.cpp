// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Digital circuits built from the logical models and nested components: the
// expected results are exact, and the size of the circuits grows quickly.

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/modeling.hpp>

#include <array>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <optional>
#include <random>
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

        irt::vector<int> rows(capacity, 0);
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

    for (int i = 0; i < Bits; ++i) {
        const auto q    = n.gate(dynamics_type::logical_d_flipflop, {});
        const auto next = n.or_(n.and_(load, d[i]), n.and_(hold, q));

        n.link(next, *q.child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *q.child, mdl(irt::logical_d_flipflop::port_clk));
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("q{}", i).sv())),
          q);
    }

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
