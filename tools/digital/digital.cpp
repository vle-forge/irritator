// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "digital.hpp"

#include <irritator/format.hpp>
#include <irritator/io.hpp>

#include <algorithm>
#include <fstream>

namespace irt::digital {

namespace {

/** A signal of a generic component: an output port of a child or, without
 * child, an input port of the component. */
struct wire {
    std::optional<irt::child_id> child;
    connection::port             port;
};

/** Allocates the gates of a generic component and connects them. The children
 * are kept by identifier: the children container grows during the build.
 *
 * The children are named to find one's way in the component editor: a gate
 * by its type, until it is named after the signal it computes or the output
 * it feeds. */
class netlist
{
public:
    netlist(const irt::component& c, irt::generic_component& gen) noexcept
      : m_compo(c)
      , m_gen(gen)
    {}

    static wire input(const irt::port_id x) noexcept
    {
        return wire{ .child = std::nullopt, .port = compo(x) };
    }

    irt::child_id child(const irt::component_id id,
                        const std::string_view  name) noexcept
    {
        irt::fatal::ensure((m_gen.grow_children()));
        const auto child            = m_gen.children.get_id(m_gen.alloc(id));
        m_gen.children_names[child] = unique_name(name);
        return child;
    }

    /** Names the gate which computes @a w, if no other name was given to it
     * before: a signal shared by several names keeps the first one. */
    const wire& name(const wire& w, const std::string_view name) noexcept
    {
        if (w.child and is_unnamed(*w.child)) {
            m_gen.children_names[*w.child]      = unique_name(name);
            m_unnamed[irt::get_index(*w.child)] = false;
        }

        return w;
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

        irt::fatal::ensure((have_room(m_gen.connections) and
                            have_room(m_gen.input_connections)));

        if (src.child)
            irt::fatal::ensure(!!m_gen.connect(m_gen.children.get(*src.child),
                                               src.port, child_dst, port));
        else
            irt::fatal::ensure(
              !!m_gen.connect_input(src.port.compo, child_dst, port));
    }

    void output(const irt::port_id y, const wire& src) noexcept
    {
        irt::fatal::ensure((src.child.has_value()));
        irt::fatal::ensure((have_room(m_gen.output_connections)));
        irt::fatal::ensure(
          !!m_gen.connect_output(y, m_gen.children.get(*src.child), src.port));
        name(src, m_compo.y.get<irt::port_str>(y).sv());
    }

    wire gate(const dynamics_type type, std::initializer_list<wire> inputs)
    {
        irt::fatal::ensure((m_gen.grow_children()));
        const auto id            = m_gen.children.get_id(m_gen.alloc(type));
        const auto index         = irt::get_index(id);
        m_gen.children_names[id] = irt::format_n<31>("{}-{}", type_name(type),
                                                     index)
                                     .sv();
        if (m_unnamed.ssize() <= static_cast<int>(index))
            m_unnamed.resize(m_gen.children.capacity(), false);
        m_unnamed[index] = true;

        int port = 0;
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
    static std::string_view type_name(const dynamics_type type) noexcept
    {
        switch (type) {
        case dynamics_type::logical_invert:
            return "not";
        case dynamics_type::logical_and_2:
        case dynamics_type::logical_and_3:
            return "and";
        case dynamics_type::logical_or_2:
        case dynamics_type::logical_or_3:
            return "or";
        case dynamics_type::logical_xor_2:
            return "xor";
        case dynamics_type::logical_d_flipflop:
            return "dff";
        default:
            return irt::dynamics_type_names[irt::ordinal(type)];
        }
    }

    bool is_unnamed(const irt::child_id id) const noexcept
    {
        const auto index = irt::get_index(id);
        return static_cast<int>(index) < m_unnamed.ssize() and m_unnamed[index];
    }

    /** The name, followed by a number if a child already has it. */
    irt::name_str unique_name(const std::string_view name) const noexcept
    {
        irt::name_str ret(name);
        for (int i = 2; m_gen.exists_child(ret.sv()); ++i)
            ret = irt::format_n<31>("{}-{}", name, i).sv();

        return ret;
    }

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

    const irt::component&   m_compo;
    irt::generic_component& m_gen;
    irt::vector<bool>       m_unnamed;
};

} // namespace

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

    netlist    n(c, gen);
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
adder_component<Bits> make_adder(irt::component_access&      ids,
                                 const full_adder_component& fa) noexcept
{
    adder_component<Bits> ret;
    ret.id    = ids.alloc_generic_component();
    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.cin   = c.get_or_add_x("cin");
    ret.cout  = c.get_or_add_y("cout");

    netlist n(c, gen);
    wire    carry = netlist::input(ret.cin);
    for (int i = 0; i < Bits; ++i) {
        ret.a[i] = c.get_or_add_x(irt::format_n<32>("a{}", i).sv());
        ret.b[i] = c.get_or_add_x(irt::format_n<32>("b{}", i).sv());
        ret.s[i] = c.get_or_add_y(irt::format_n<32>("s{}", i).sv());

        const auto bit = n.child(fa.id, irt::format_n<8>("fa{}", i).sv());
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

template adder_component<4> make_adder<4>(
  irt::component_access&      ids,
  const full_adder_component& fa) noexcept;

template adder_component<8> make_adder<8>(
  irt::component_access&      ids,
  const full_adder_component& fa) noexcept;

template adder_component<16> make_adder<16>(
  irt::component_access&      ids,
  const full_adder_component& fa) noexcept;

circuit make_alu(irt::component_access&    ids,
                 const adder_component<8>& adder) noexcept
{
    circuit ret{ .id = ids.alloc_generic_component(),
                 .x  = irt::vector<irt::port_id>(21, irt::reserve_tag),
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
    const auto decimal = netlist::input(
      *ret.x.emplace_back(c.get_or_add_x("d")));

    netlist n(c, gen);

    const std::array<wire, 3> not_op{ n.not_(op[0]), n.not_(op[1]),
                                      n.not_(op[2]) };

    std::array<wire, alu_op_count> select;
    for (unsigned k = 0; k < alu_op_count; ++k)
        select[k] = n.and_(k & 1u ? op[0] : not_op[0],
                           k & 2u ? op[1] : not_op[1],
                           k & 4u ? op[2] : not_op[2]);

    constexpr std::array<const char*, alu_op_count> op_names{
        "ora", "and", "eor", "adc", "sbc", "rol", "ror", "pass_b"
    };
    for (unsigned k = 0; k < alu_op_count; ++k)
        n.name(select[k], op_names[k]);

    const auto is = [&](const alu_op o) { return select[static_cast<int>(o)]; };

    const auto sub   = is(alu_op::sbc);
    const auto arith = n.or_(is(alu_op::adc), sub);

    const auto          add = n.child(adder.id, "adder");
    std::array<wire, 8> b_add, s;
    for (int i = 0; i < 8; ++i) {
        b_add[i] = n.xor_(b[i], sub);
        n.link(a[i], add, compo(adder.a[i]));
        n.link(b_add[i], add, compo(adder.b[i]));
        s[i] = netlist::output_of(add, adder.s[i]);
    }
    n.link(cin, add, compo(adder.cin));
    const auto cout = netlist::output_of(add, adder.cout);

    // The decimal correction. The carry into the bit 4 is recovered from the
    // sum. ADC adds 6 to a digit above 9, carried by the low digit into the
    // high one, SBC adds 10 (subtracts 6) to a digit which borrowed. The
    // high digit before its correction, ah, gives N and V to ADC; without
    // correction it is the binary sum, so the binary results go through the
    // same gates.
    const auto above_9 = [&](const wire& carry, const wire& b3, const wire& b2,
                             const wire& b1) {
        return n.or_(carry, n.and_(b3, n.or_(b2, b1)));
    };

    const auto correct = [&](const std::array<wire, 4>& digit, const wire& six,
                             const wire& ten) {
        const auto two = n.or_(six, ten);
        const auto c1  = n.and_(digit[1], two);
        const auto x2  = n.xor_(digit[2], six);
        const auto c2  = n.or_(n.and_(digit[2], six), n.and_(x2, c1));
        return std::array<wire, 4>{ digit[0], n.xor_(digit[1], two),
                                    n.xor_(x2, c1),
                                    n.xor_(n.xor_(digit[3], ten), c2) };
    };

    const auto decimal_adc = n.and_(decimal, is(alu_op::adc));
    const auto decimal_sbc = n.and_(decimal, sub);
    const auto c4          = n.xor_(s[4], n.xor_(a[4], b_add[4]));
    const auto low_carry   = above_9(c4, s[3], s[2], s[1]);

    std::array<wire, 5> ah;
    auto step = n.and_(decimal_adc, n.and_(low_carry, n.not_(c4)));
    for (int i = 0; i < 5; ++i) {
        const auto bit = i < 4 ? s[4 + i] : cout;
        ah[i]          = n.xor_(bit, step);
        if (i < 4)
            step = n.and_(bit, step);
    }
    const auto high_carry = above_9(ah[4], ah[3], ah[2], ah[1]);

    const auto low  = correct({ s[0], s[1], s[2], s[3] },
                              n.and_(decimal_adc, low_carry),
                              n.and_(decimal_sbc, n.not_(c4)));
    const auto high = correct({ ah[0], ah[1], ah[2], ah[3] },
                              n.and_(decimal_adc, high_carry),
                              n.and_(decimal_sbc, n.not_(cout)));

    std::array<wire, 8> r;
    for (int i = 0; i < 8; ++i) {
        const std::array candidates{
            n.and_(is(alu_op::ora), n.or_(a[i], b[i])),
            n.and_(is(alu_op::and_), n.and_(a[i], b[i])),
            n.and_(is(alu_op::eor), n.xor_(a[i], b[i])),
            n.and_(arith, i < 4 ? low[i] : high[i - 4]),
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
        n.and_(n.and_(arith, n.not_(decimal_adc)), cout),
        n.and_(decimal_adc, high_carry),
        n.and_(is(alu_op::rol), a[7]),
        n.and_(is(alu_op::ror), a[0]),
    };

    const auto overflow = n.and_(
      arith, n.and_(n.xor_(a[7], ah[3]), n.xor_(b_add[7], ah[3])));

    const auto          logic = n.not_(arith);
    std::array<wire, 8> zero;
    for (int i = 0; i < 8; ++i)
        zero[i] = n.or_(n.and_(arith, s[i]), n.and_(logic, r[i]));

    for (int i = 0; i < 8; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("r{}", i).sv())),
          r[i]);

    n.output(*ret.y.emplace_back(c.get_or_add_y("c")), n.any(carry));
    n.output(*ret.y.emplace_back(c.get_or_add_y("v")), overflow);
    n.output(*ret.y.emplace_back(c.get_or_add_y("n")),
             n.or_(n.and_(arith, ah[3]), n.and_(logic, r[7])));
    n.output(*ret.y.emplace_back(c.get_or_add_y("z")), n.not_(n.any(zero)));
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

    netlist    n(c, gen);
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

template circuit make_counter<8>(irt::component_access& ids) noexcept;

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

    netlist    n(c, gen);
    const auto hold = n.name(n.not_(load), "hold");

    for (int i = 0; i < Bits; ++i)
        n.output(
          *ret.y.emplace_back(c.get_or_add_y(irt::format_n<8>("q{}", i).sv())),
          make_loadable_flipflop(n, d[i], load, hold, clk));

    n.place();

    return ret;
}

template circuit make_register<8>(irt::component_access& ids) noexcept;

circuit make_accumulator(irt::component_access& ids,
                         const circuit&         alu,
                         const circuit&         reg) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(14, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(12, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    netlist    n(c, gen);
    const auto u = n.child(alu.id, "alu");
    const auto a = n.child(reg.id, "a");

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
    n.link(netlist::input(*ret.x.emplace_back(c.get_or_add_x("d"))), u,
           compo(alu.x[20]));

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

circuit make_flags(irt::component_access& ids) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(11, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(6, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    const auto input = [&](const std::string_view name) {
        return netlist::input(*ret.x.emplace_back(c.get_or_add_x(name)));
    };

    constexpr std::array<const char*, 6> names{ "c", "v", "n", "z", "i", "d" };
    std::array<wire, 6>                  d;
    std::array<wire, 5>                  load;
    for (int i = 0; i < 4; ++i)
        d[i] = input(names[i]);
    load[0] = input("load_c");
    load[1] = input("load_v");
    load[2] = input("load_nz");
    d[4]    = input("i");
    d[5]    = input("d");
    load[3] = input("load_i");
    load[4] = input("load_d");

    ret.clk        = c.get_or_add_x("clk");
    const auto clk = netlist::input(*ret.clk);

    netlist             n(c, gen);
    std::array<wire, 5> hold;
    for (int i = 0; i < 5; ++i)
        hold[i] = n.not_(load[i]);

    // The flags c, v, n, z, i and d use the loads 0, 1, 2, 2, 3 and 4.
    constexpr std::array<int, 6> load_of{ 0, 1, 2, 2, 3, 4 };
    for (int i = 0; i < 6; ++i) {
        const auto l = load_of[i];
        n.output(*ret.y.emplace_back(c.get_or_add_y(names[i])),
                 make_loadable_flipflop(n, d[i], load[l], hold[l], clk));
    }

    n.place();

    return ret;
}

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

    netlist    n(c, gen);
    const auto a = n.child(acc.id, "accumulator");
    const auto p = n.child(flags.id, "p");

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

    netlist n(c, gen);

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

circuit make_memory(irt::component_access&   ids,
                    std::span<const irt::u8> content,
                    const irt::file_path_id  file) noexcept
{
    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(25, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(8, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);

    netlist    n(c, gen);
    const auto mem = n.name(n.gate(dynamics_type::logical_memory, {}), "ram");

    if (not content.empty()) {
        irt::fatal::ensure(
          (c.srcs.data.can_alloc(1) or c.srcs.data.reserve(1)));
        const auto src_id = c.srcs.data.alloc_id();
        auto&      src    = c.srcs.data
                      .get<irt::external_source_definition::source_element>(
                        src_id);

        if (irt::is_defined(file)) {
            c.srcs.emplace(src_id, irt::source_type::binary_file, "program");
            src.bin.file = file;
        } else {
            c.srcs.emplace(src_id, irt::source_type::constant, "content");
            irt::fatal::ensure((src.cst.data.resize(content.size())));
            std::copy(content.begin(), content.end(), src.cst.data.begin());
        }

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

    netlist    n(c, gen);
    const auto x     = n.child(reg.id, "x");
    const auto y     = n.child(reg.id, "y");
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

    netlist    n(c, gen);
    const auto s    = n.child(reg.id, "s");
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

circuit make_cpu(irt::component_access& ids, const cpu_parts& parts) noexcept
{
    const auto& [pc, memory, accumulator, flags, index, stack, reg, adder,
                 comparator, alu_unit] = parts;

    circuit ret{ .id  = ids.alloc_generic_component(),
                 .x   = irt::vector<irt::port_id>(2, irt::reserve_tag),
                 .y   = irt::vector<irt::port_id>(55, irt::reserve_tag),
                 .clk = std::nullopt };

    auto& c   = ids.components[ret.id];
    auto& gen = ids.generic_components.get(c.id.generic_id);
    ret.clk   = c.get_or_add_x("clk");

    netlist    n(c, gen);
    const auto clk = netlist::input(*ret.clk);
    const auto irq = netlist::input(*ret.x.emplace_back(c.get_or_add_x("irq")));
    const auto nmi = netlist::input(*ret.x.emplace_back(c.get_or_add_x("nmi")));
    const auto p   = n.child(pc.id, "pc");
    const auto mem = n.child(memory.id, "memory");
    const auto acc = n.child(accumulator.id, "accumulator");
    const auto f   = n.child(flags.id, "p");
    const auto idx = n.child(index.id, "xy");
    const auto sp  = n.child(stack.id, "s");
    const auto ir  = n.child(reg.id, "ir");
    const auto adl = n.child(reg.id, "adl");
    const auto adh = n.child(reg.id, "adh");
    const auto add = n.child(adder.id, "adder");
    const auto cmp = n.child(comparator.id, "comparator");
    const auto dl  = n.child(reg.id, "dl");
    const auto rmw = n.child(alu_unit.id, "rmw");

    const auto bit = [](const irt::child_id child, const circuit& of,
                        const int i) {
        return netlist::output_of(child, of.y[i]);
    };

    const auto link_clk = [&](const irt::child_id child, const circuit& of) {
        n.link(clk, child, compo(*of.clk));
    };

    // The decoder. The operations of the ALU, CMP and STA have the bits 1..0
    // = 01 and the addressing mode in the bits 4..2: bit 2 without bit 3
    // tells the zero page, bit 3 with bit 2 or 4 an absolute address, bit 4
    // adds X with bit 2 or Y without, and neither bit 3 nor bit 2 a pointer
    // in the zero page. The other opcodes are recognized on their eight bits.
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

    // The read-modify-write operations have the bits 1..0 = 10, the modes of
    // the group 01 but always the bit 2, and the bits 7..5 neither 100 (STX)
    // nor 101 (LDX).
    const auto group     = n.and_(ir_bits[0], ir_inverse[1]);
    const auto modify_op = n.all(std::array{
      ir_inverse[0], ir_bits[1], ir2, n.not_(n.and_(ir7, ir_inverse[6])) });
    const auto moded     = n.or_(group, modify_op);
    const auto store = n.all(std::array{ ir7, ir_inverse[6], ir_inverse[5] });
    const auto cmp_a = n.and_(group,
                              n.all(std::array{ ir7, ir6, ir_inverse[5] }));
    const auto sta   = n.and_(group, store);
    const auto alu   = n.and_(group, n.not_(n.or_(store, cmp_a)));
    const auto carry = n.and_(alu, n.and_(ir6, ir5));

    const auto pointer = n.all(
      std::array{ group, ir_inverse[3], ir_inverse[2] });
    const auto pointer_x = n.and_(pointer, ir_inverse[4]);
    const auto pointer_y = n.and_(pointer, ir4);

    const auto group_zero_page = n.and_(moded, n.and_(ir2, ir_inverse[3]));
    const auto group_absolute  = n.and_(moded, n.and_(ir3, n.or_(ir2, ir4)));
    const auto group_index_x   = n.and_(moded, n.and_(ir4, ir2));
    const auto group_index_y   = n.all(
      std::array{ group, ir4, ir3, ir_inverse[2] });

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

    // LDX and LDY in the zero page or at an absolute address plus an index,
    // Y for LDX and X for LDY.
    std::array<std::array<wire, 2>, 2> indexed_loads;
    for (int i = 0; i < 2; ++i)
        for (int m = 0; m < 2; ++m)
            indexed_loads[i][m] = match(with_mode(
              index_load_and_compare_opcodes[i],
              m == 0 ? addressing::zero_page_x : addressing::absolute_x));
    const auto  ldx = n.any(std::array{
      index_instructions[0], indexed_loads[0][0], indexed_loads[0][1] });
    const auto  ldy = n.any(std::array{
      index_instructions[1], indexed_loads[1][0], indexed_loads[1][1] });
    const auto& cpx = index_instructions[2];
    const auto& cpy = index_instructions[3];

    // STX and STY: the zero page, the zero page plus an index then the
    // absolute modes.
    constexpr std::array               store_modes{ addressing::zero_page,
                                      addressing::zero_page_x,
                                      addressing::absolute };
    std::array<std::array<wire, 3>, 2> index_stores;
    for (int i = 0; i < 2; ++i)
        for (int m = 0; m < 3; ++m)
            index_stores[i][m] = match(
              with_mode(index_store_opcodes[i], store_modes[m]));
    const auto stx = n.any(index_stores[0]);
    const auto sty = n.any(index_stores[1]);

    const auto bit_zero_page = match(opcode::bit_zp);
    const auto bit_absolute  = match(
      with_mode(opcode::bit_zp, addressing::absolute));
    const auto bit_op   = n.or_(bit_zero_page, bit_absolute);
    const auto indirect = match(opcode::jmp_ind);

    const auto zero_page_indexed = n.any(std::array{
      n.and_(group_zero_page, ir4), indexed_loads[0][0], indexed_loads[1][0],
      index_stores[0][1], index_stores[1][1] });
    const auto indexed_absolute  = n.any(std::array{
      n.and_(group_absolute, ir4), indexed_loads[0][1], indexed_loads[1][1] });

    // JMP ($addr) reads its pointer as the operand of an absolute address.
    const auto zero_page = n.any(
      std::array{ group_zero_page, n.any(by_mode[1]), zero_page_indexed,
                  index_stores[0][0], index_stores[1][0], bit_zero_page });
    const auto absolute  = n.any(std::array{
      group_absolute, n.any(by_mode[2]), indexed_absolute, index_stores[0][2],
      index_stores[1][2], bit_absolute, indirect });
    const auto direct    = n.or_(zero_page, absolute);
    const auto addressed = n.or_(direct, pointer);
    const auto compare   = n.any(std::array{ cmp_a, cpx, cpy });

    std::array<wire, 7> implied_index_ops;
    for (int i = 0; i < 7; ++i)
        implied_index_ops[i] = match(index_opcodes[i]);
    const auto& [inx, iny, dex, dey, tsx, tax, tay] = implied_index_ops;
    const auto txa                                  = match(opcode::txa);
    const auto tya                                  = match(opcode::tya);
    const auto to_a                                 = n.or_(txa, tya);
    const auto implied_index                        = n.any(implied_index_ops);
    const auto index_load                           = n.or_(ldx, ldy);
    const auto index_op = n.or_(implied_index, index_load);

    std::array<wire, 4> shifts;
    for (int i = 0; i < 4; ++i)
        shifts[i] = match(shift_opcodes[i]);
    const auto& [asl, rol, lsr, ror] = shifts;
    const auto shift                 = n.any(shifts);
    const auto shift_left            = n.or_(asl, rol);
    const auto shift_in_zero         = n.or_(asl, lsr);

    // The branches have the bits 4..0 = 10000.
    const auto jmp        = match(opcode::jmp_abs);
    const auto branch     = n.all(std::array{ ir_inverse[0], ir_inverse[1],
                                          ir_inverse[2], ir_inverse[3], ir4 });
    const auto clc        = match(opcode::clc);
    const auto sec        = match(opcode::sec);
    const auto clv        = match(opcode::clv);
    const auto cli        = match(opcode::cli);
    const auto sei        = match(opcode::sei);
    const auto cld        = match(opcode::cld);
    const auto sed        = match(opcode::sed);
    const auto set_c      = n.or_(clc, sec);
    const auto set_i      = n.or_(cli, sei);
    const auto set_d      = n.or_(cld, sed);
    const auto pha        = match(opcode::pha);
    const auto pla        = match(opcode::pla);
    const auto php        = match(opcode::php);
    const auto plp        = match(opcode::plp);
    const auto jsr        = match(opcode::jsr_abs);
    const auto rts        = match(opcode::rts);
    const auto brk        = match(opcode::brk);
    const auto rti        = match(opcode::rti);
    const auto txs        = match(opcode::txs);
    const auto nop        = match(opcode::nop);
    const auto push_op    = n.or_(pha, php);
    const auto pull_op    = n.or_(pla, plp);
    const auto pulls_p    = n.or_(plp, rti);
    const auto subroutine = n.any(std::array{ jsr, rts, brk, rti });
    const auto stack_op   = n.any(std::array{ push_op, pull_op, subroutine });
    const auto implied    = n.any(std::array{ implied_index, to_a, txs, push_op,
                                           pull_op, rts, rti, shift, set_c, clv,
                                           set_i, set_d, nop });

    // The bits 7..6 select N, V, C or Z: the branch is taken if the flag
    // equals the bit 5.
    const auto tested = n.any(std::array{
      n.and_(n.and_(ir_inverse[7], ir_inverse[6]), bit(f, flags, 2)),
      n.and_(n.and_(ir_inverse[7], ir6), bit(f, flags, 1)),
      n.and_(n.and_(ir7, ir_inverse[6]), bit(f, flags, 0)),
      n.and_(n.and_(ir7, ir6), bit(f, flags, 3)) });
    const auto taken  = n.and_(branch, n.not_(n.xor_(tested, ir5)));

    std::array<wire, 6> t;
    for (int i = 0; i < 6; ++i)
        t[i] = n.name(n.gate(dynamics_type::logical_d_flipflop, {}),
                      irt::format_n<8>("t{}", i + 1).sv());

    // The pointer of ($zp,X) is read in t3 and t4, of ($zp),Y in t2 and t3,
    // then dl and adh hold the address of the operand.
    const auto through_pointer = n.or_(n.and_(pointer_x, t[4]),
                                       n.and_(pointer_y, n.or_(t[3], t[4])));
    const auto reading_pointer = n.and_(pointer, n.not_(through_pointer));
    const auto index_x         = n.any(
      std::array{ group_index_x, indexed_loads[1][0], indexed_loads[1][1],
                  index_stores[1][1], n.and_(pointer_x, reading_pointer) });
    const auto index_y = n.any(
      std::array{ group_index_y, indexed_loads[0][0], indexed_loads[0][1],
                  index_stores[0][1], n.and_(pointer_y, through_pointer) });

    // The adder: a0..a15, b0..b15 then cin. A branch adds the offset in adl,
    // extended to 16 bits, to pc. Otherwise X, Y or zero is added to the
    // address in adh and adl, or dl after a pointer. The carry reads the
    // high byte of a pointer.
    const auto          no_branch = n.not_(branch);
    const auto          from_adl  = n.not_(through_pointer);
    std::array<wire, 8> high;
    for (int i = 0; i < 16; ++i) {
        const auto address = i < 8
                               ? n.or_(n.and_(from_adl, bit(adl, reg, i)),
                                       n.and_(through_pointer, bit(dl, reg, i)))
                               : bit(adh, reg, i - 8);
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
    n.link(
      n.or_(n.and_(t[3], n.or_(indirect, pointer_x)), n.and_(t[2], pointer_y)),
      add, compo(adder.x[32]));

    std::array<wire, 8> page_changes;
    for (int i = 0; i < 8; ++i)
        page_changes[i] = n.xor_(bit(add, adder, 8 + i), high[i]);
    const auto page_crossed = n.any(page_changes);

    // The sequencer. The operand is read in t2 at a zero page address, in t3
    // at a zero page address plus an index or at an absolute address, in t4
    // at an absolute address plus an index in the next page or after the
    // pointer of ($zp),Y, in t5 after the pointer of ($zp,X) or in the next
    // page after the pointer of ($zp),Y. A read-modify-write operation always
    // takes the extra cycle of an index, then modifies the byte read in the
    // next cycle and writes it in the one after: the flip-flops modifying and
    // writing_back follow these two cycles.
    const auto t0           = n.not_(n.any(t));
    const auto modifying    = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto writing_back = n.gate(dynamics_type::logical_d_flipflop, {});

    const auto one_more = n.and_(
      n.or_(indexed_absolute, pointer_y),
      n.any(std::array{ sta, modify_op, page_crossed }));
    const auto last = n.and_(
      n.not_(n.or_(modifying, writing_back)),
      n.or_(
        n.and_(direct,
               n.any(std::array{
                 n.and_(t[1], n.and_(zero_page, n.not_(zero_page_indexed))),
                 n.and_(t[2], n.not_(one_more)), t[3] })),
        n.and_(pointer, n.or_(n.and_(t[3], n.and_(pointer_y, n.not_(one_more))),
                              t[4]))));
    const auto modified = n.and_(modify_op, n.or_(last, modifying));

    // An edge of nmi waits for the next fetch. The flip-flops hardware and
    // to_nmi keep, until the next fetch, whether the BRK replaces an opcode
    // and which vector it reads.
    const auto nmi_before   = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto nmi_waiting  = n.gate(dynamics_type::logical_d_flipflop, {});
    const auto nmi_request  = n.or_(n.and_(nmi, n.not_(nmi_before)),
                                    nmi_waiting);
    const auto request      = n.or_(nmi_request,
                                    n.and_(irq, n.not_(bit(f, flags, 4))));
    const auto interrupting = n.and_(t0, request);
    const auto not_t0       = n.not_(t0);
    const auto hardware = make_loadable_flipflop(n, request, t0, not_t0, clk);
    const auto to_nmi = make_loadable_flipflop(n, nmi_request, t0, not_t0, clk);
    for (const auto& [ff, d] :
         { std::pair{ nmi_before, nmi },
           std::pair{ nmi_waiting, n.and_(nmi_request, not_t0) } }) {
        n.link(d, *ff.child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *ff.child, mdl(irt::logical_d_flipflop::port_clk));
    }

    const std::array<wire, 6> next{
        t0,
        n.and_(t[0], n.any(std::array{ addressed, jmp, taken, stack_op })),
        n.and_(t[1], n.any(std::array{ absolute, zero_page_indexed,
                                       n.and_(branch, page_crossed), pull_op,
                                       subroutine, modified, pointer })),
        n.and_(t[2], n.any(std::array{ one_more, subroutine, modified, indirect,
                                       pointer })),
        n.and_(t[3], n.any(std::array{ subroutine, modified, pointer_x,
                                       n.and_(pointer_y, one_more) })),
        n.and_(t[4], n.or_(modified, brk))
    };
    for (int i = 0; i < 6; ++i) {
        n.link(next[i], *t[i].child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *t[i].child, mdl(irt::logical_d_flipflop::port_clk));
    }
    for (const auto& [ff, d] :
         { std::pair{ modifying, n.and_(last, modify_op) },
           std::pair{ writing_back, modifying } }) {
        n.link(d, *ff.child, mdl(irt::logical_d_flipflop::port_d));
        n.link(clk, *ff.child, mdl(irt::logical_d_flipflop::port_clk));
    }

    // The operand of an operation is read in t1 without address, at the end
    // of the address otherwise.
    const auto operand   = n.or_(n.and_(t[0], n.not_(addressed)), last);
    const auto execute   = n.and_(operand, alu);
    const auto shifted   = n.and_(t[0], shift);
    const auto moved     = n.and_(t[0], to_a);
    const auto pull_a    = n.and_(t[2], pla);
    const auto pulled_p  = n.and_(t[2], pulls_p);
    const auto compared  = n.and_(operand, compare);
    const auto push_high = n.or_(n.and_(jsr, t[2]), n.and_(brk, t[1]));
    const auto push_low  = n.or_(n.and_(jsr, t[3]), n.and_(brk, t[2]));
    const auto push_p    = n.or_(n.and_(php, t[1]), n.and_(brk, t[3]));
    const auto pushed    = n.any(
      std::array{ n.and_(pha, t[1]), push_high, push_low, push_p });
    const auto vector = n.and_(brk, n.or_(t[4], t[5]));

    const auto load_adl = n.any(std::array{
      n.and_(t[0], n.any(std::array{ addressed, jmp, branch, jsr })),
      n.and_(t[2], rts), n.and_(t[3], rti) });
    const auto load_adh = n.any(std::array{ n.and_(t[1], absolute),
                                            n.and_(t[3], pointer_x),
                                            n.and_(t[2], pointer_y) });
    const auto load_dl  = n.any(std::array{ last, n.and_(t[2], pointer_x),
                                           n.and_(t[1], pointer_y),
                                           n.and_(t[4], brk) });
    const auto jump     = n.any(std::array{
      n.and_(t[1], n.or_(jmp, branch)), n.and_(t[4], n.or_(jsr, rti)),
      n.and_(t[3], n.or_(rts, indirect)), n.and_(t[5], brk) });
    const auto stores   = n.any(std::array{ sta, stx, sty });
    const auto write    = n.any(
      std::array{ n.and_(last, stores), pushed, writing_back });

    // The registers: d0..d7 then load. An interrupt loads BRK, 0x00, in ir.
    const auto fetched = n.not_(interrupting);
    for (int i = 0; i < 8; ++i)
        n.link(n.and_(fetched, bit(mem, memory, i)), ir, compo(reg.x[i]));
    n.link(t0, ir, compo(reg.x[8]));
    link_clk(ir, reg);

    const std::array registers{ std::pair{ adl, load_adl },
                                std::pair{ adh, load_adh },
                                std::pair{ dl, load_dl } };
    for (const auto& [r, load] : registers) {
        for (int i = 0; i < 8; ++i)
            n.link(bit(mem, memory, i), r, compo(reg.x[i]));
        n.link(load, r, compo(reg.x[8]));
        link_clk(r, reg);
    }

    // The program counter: d0..d15, load then inc. JMP, JSR, RTS and RTI take
    // adl and the byte read, JMP ($addr) and BRK the byte read before, a
    // branch the target. pc + 1 in t0 but for an interrupt, t1 but for the
    // implied operations and the BRK of an interrupt, t2 but for the stack,
    // the zero page and the pointers, and t5 of RTS: a load wins.
    const auto from_address = n.any(std::array{ jmp, jsr, rts, rti });
    const auto from_dl      = n.or_(indirect, brk);
    const auto from_memory  = n.or_(from_address, from_dl);
    const auto from_adder   = n.not_(from_memory);
    for (int i = 0; i < 16; ++i) {
        const auto high = i < 8 ? n.or_(n.and_(from_address, bit(adl, reg, i)),
                                        n.and_(from_dl, bit(dl, reg, i)))
                                : n.and_(from_memory, bit(mem, memory, i - 8));
        n.link(n.or_(high, n.and_(from_adder, bit(add, adder, i))), p,
               compo(pc.x[i]));
    }
    n.link(jump, p, compo(pc.x[16]));
    n.link(
      n.not_(n.any(std::array{
        interrupting, t[2], t[3], t[5], n.and_(t[4], n.not_(rts)),
        n.and_(t[0], n.or_(implied, hardware)),
        n.and_(t[1], n.any(std::array{ stack_op, zero_page, pointer })) })),
      p, compo(pc.x[17]));
    link_clk(p, pc);

    // The accumulator: b0..b7, cin, op0..op2, load then d. The operation is
    // the bits 7..5 of the opcode, except LDA (101), PLA, TXA and TYA (100)
    // which become pass_b (111), SBC (111) which becomes sbc (100), ASL and
    // ROL which become rol (101) and LSR and ROR which become ror (110). ASL
    // and LSR shift in a zero. TXA and TYA pass X or Y instead of the bus.
    const auto from_data = n.not_(to_a);
    for (int i = 0; i < 8; ++i)
        n.link(n.any(std::array{ n.and_(from_data, bit(mem, memory, i)),
                                 n.and_(txa, bit(idx, index, i)),
                                 n.and_(tya, bit(idx, index, 8 + i)) }),
               acc, compo(accumulator.x[i]));
    n.link(n.and_(bit(f, flags, 0), n.not_(shift_in_zero)), acc,
           compo(accumulator.x[8]));
    n.link(n.any(std::array{
             n.and_(n.not_(shift), n.and_(ir5, n.not_(n.and_(ir7, ir6)))),
             shift_left, to_a }),
           acc, compo(accumulator.x[9]));
    n.link(n.xor_(ir7, ir6), acc, compo(accumulator.x[10]));
    n.link(n.any(std::array{ ir7, pla, shift }), acc, compo(accumulator.x[11]));
    n.link(n.any(std::array{ execute, pull_a, shifted, moved }), acc,
           compo(accumulator.x[12]));
    n.link(bit(f, flags, 5), acc, compo(accumulator.x[13]));
    link_clk(acc, accumulator);

    // The index registers: d0..d7, imm, dec, sel_y then write.
    const auto index_execute = n.or_(n.and_(t[0], implied_index),
                                     n.and_(operand, index_load));
    const auto from_a        = n.or_(tax, tay);
    const auto from_bus      = n.not_(n.or_(tsx, from_a));
    for (int i = 0; i < 8; ++i)
        n.link(n.any(std::array{ n.and_(tsx, bit(sp, stack, i)),
                                 n.and_(from_a, bit(acc, accumulator, i)),
                                 n.and_(from_bus, bit(mem, memory, i)) }),
               idx, compo(index.x[i]));
    n.link(n.any(std::array{ index_load, tsx, from_a }), idx,
           compo(index.x[8]));
    n.link(n.or_(dex, dey), idx, compo(index.x[9]));
    n.link(n.any(std::array{ ldy, iny, dey, tay }), idx, compo(index.x[10]));
    n.link(index_execute, idx, compo(index.x[11]));
    link_clk(idx, index);

    // The stack pointer: d0..d7, load, inc then dec.
    for (int i = 0; i < 8; ++i)
        n.link(bit(idx, index, i), sp, compo(stack.x[i]));
    n.link(n.and_(t[0], txs), sp, compo(stack.x[8]));
    n.link(
      n.any(std::array{ n.and_(t[1], pull_op), n.and_(rts, n.or_(t[1], t[2])),
                        n.and_(rti, n.any(std::array{ t[1], t[2], t[3] })) }),
      sp, compo(stack.x[9]));
    n.link(pushed, sp, compo(stack.x[10]));
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

    // The second ALU modifies the byte read: a0..a7, b0..b7 left false, cin
    // then op0..op2. The bits 7..5 of the opcode, ASL (000), ROL (001), LSR
    // (010), ROR (011), DEC (110) and INC (111), become rol (101), ror (110),
    // sbc (100) of zero, the byte minus one, and adc (011) of zero plus one.
    const auto increment = n.all(std::array{ ir7, ir6, ir5 });
    const auto rotate    = n.and_(ir_inverse[7], ir5);
    for (int i = 0; i < 8; ++i)
        n.link(bit(dl, reg, i), rmw, compo(alu_unit.x[i]));
    n.link(n.or_(n.and_(rotate, bit(f, flags, 0)), increment), rmw,
           compo(alu_unit.x[16]));
    n.link(n.or_(n.and_(ir_inverse[7], ir_inverse[6]), increment), rmw,
           compo(alu_unit.x[17]));
    n.link(n.or_(n.and_(ir_inverse[7], ir6), increment), rmw,
           compo(alu_unit.x[18]));
    n.link(n.not_(increment), rmw, compo(alu_unit.x[19]));

    // The flags: c, v, n, z, load_c, load_v, load_nz, i, d, load_i then
    // load_d. N and Z come from the index registers, the comparator, the
    // second ALU, BIT, the byte pulled or the ALU. Only the shifts of the
    // memory change C. PLP and RTI pull P: c, z, i, d, b, 1, v then n.
    const auto from_alu = n.not_(
      n.any(std::array{ index_op, compare, modify_op, bit_op, pulls_p }));
    const auto pulled = [&](const int i) {
        return n.and_(pulls_p, bit(mem, memory, i));
    };
    n.link(
      n.any(std::array{
        n.and_(compare, bit(cmp, comparator, 8)),
        n.and_(modify_op, bit(rmw, alu_unit, 8)), sec, pulled(0),
        n.and_(n.not_(n.any(std::array{ compare, modify_op, set_c, pulls_p })),
               bit(acc, accumulator, 8)) }),
      f, compo(flags.x[0]));
    n.link(n.any(std::array{
             n.and_(bit_op, bit(mem, memory, 6)), pulled(6),
             n.and_(n.not_(n.any(std::array{ bit_op, clv, pulls_p })),
                    bit(acc, accumulator, 9)) }),
           f, compo(flags.x[1]));
    const std::array<wire, 2> nz{ difference[7], n.not_(n.any(difference)) };

    // BIT: N and V are the bits 7 and 6 of the byte read, Z tells whether A
    // and the byte have no bit in common.
    std::array<wire, 8> common;
    for (int i = 0; i < 8; ++i)
        common[i] = n.and_(bit(acc, accumulator, i), bit(mem, memory, i));
    const std::array<wire, 2> tested_nz{ bit(mem, memory, 7),
                                         n.not_(n.any(common)) };
    for (int i = 0; i < 2; ++i)
        n.link(
          n.any(std::array{
            n.and_(index_op, bit(idx, index, 16 + i)), n.and_(compare, nz[i]),
            n.and_(modify_op, bit(rmw, alu_unit, 10 + i)),
            n.and_(bit_op, tested_nz[i]), pulled(i == 0 ? 7 : 1),
            n.and_(from_alu, bit(acc, accumulator, 10 + i)) }),
          f, compo(flags.x[2 + i]));
    const auto load_cv     = n.and_(execute, carry);
    const auto tested_bits = n.and_(last, bit_op);
    n.link(n.any(std::array{ load_cv, compared, shifted,
                             n.and_(writing_back, ir_inverse[7]),
                             n.and_(t[0], set_c), pulled_p }),
           f, compo(flags.x[4]));
    n.link(
      n.any(std::array{ load_cv, tested_bits, n.and_(t[0], clv), pulled_p }), f,
      compo(flags.x[5]));
    n.link(n.any(std::array{ execute, pull_a, index_execute, compared, shifted,
                             moved, writing_back, tested_bits, pulled_p }),
           f, compo(flags.x[6]));
    n.link(n.any(std::array{ pulled(2), sei, brk }), f, compo(flags.x[7]));
    n.link(n.or_(pulled(3), sed), f, compo(flags.x[8]));
    n.link(
      n.any(std::array{ n.and_(t[0], set_i), pulled_p, n.and_(t[4], brk) }), f,
      compo(flags.x[9]));
    n.link(n.or_(n.and_(t[0], set_d), pulled_p), f, compo(flags.x[10]));
    link_clk(f, flags);

    // The memory: a0..a15, d0..d7 then we. The address is pc, the sum of the
    // adder at the end of an address, without its high byte in the zero page,
    // or 0x0100 plus S in the stack, or the vector of BRK. JSR and BRK push
    // the high byte of pc then the low byte, PHP and BRK push P, STX and STY
    // write X and Y, a read-modify-write operation the second ALU, STA and
    // PHA write A.
    const auto in_stack = n.or_(
      n.and_(stack_op, n.any(std::array{ t[1], t[2], t[3] })),
      n.and_(rti, t[4]));
    const auto effective = n.and_(
      addressed, n.any(std::array{ n.and_(t[1], n.or_(zero_page, pointer)),
                                   t[2], t[3], t[4], t[5] }));
    // The pointer of JMP ($addr) does not carry into its high byte.
    const auto effective_high = n.and_(
      effective,
      n.not_(n.any(std::array{ zero_page, indirect, reading_pointer })));
    const auto pointer_high = n.and_(effective, indirect);
    const auto fetch = n.not_(n.any(std::array{ in_stack, effective, vector }));
    for (int i = 0; i < 16; ++i) {
        auto address = n.or_(
          i < 8 ? n.and_(effective, bit(add, adder, i))
                : n.or_(n.and_(effective_high, bit(add, adder, i)),
                        n.and_(pointer_high, high[i - 8])),
          n.and_(fetch, bit(p, pc, i)));
        if (i < 8)
            address = n.or_(n.and_(in_stack, bit(sp, stack, i)), address);
        else if (i == 8)
            address = n.or_(in_stack, address);

        // The vector: 0xfffe or 0xfffa for NMI, then the next address.
        address = n.or_(address, i == 0   ? n.and_(brk, t[5])
                                 : i == 2 ? n.and_(vector, n.not_(to_nmi))
                                          : vector);

        n.link(address, mem, compo(memory.x[i]));
    }

    const std::array<wire, 8> status{ bit(f, flags, 0), bit(f, flags, 3),
                                      bit(f, flags, 4), bit(f, flags, 5),
                                      n.not_(hardware), push_p,
                                      bit(f, flags, 1), bit(f, flags, 2) };
    const auto                push_a = n.or_(sta, pha);
    for (int i = 0; i < 8; ++i)
        n.link(n.any(std::array{ n.and_(push_a, bit(acc, accumulator, i)),
                                 n.and_(stx, bit(idx, index, i)),
                                 n.and_(sty, bit(idx, index, 8 + i)),
                                 n.and_(modify_op, bit(rmw, alu_unit, i)),
                                 n.and_(push_high, bit(p, pc, 8 + i)),
                                 n.and_(push_low, bit(p, pc, i)),
                                 n.and_(push_p, status[i]) }),
               mem, compo(memory.x[16 + i]));
    n.link(write, mem, compo(memory.x[24]));
    link_clk(mem, memory);

    for (const auto& [name, w] :
         { std::pair{ "group", group },
           std::pair{ "modify_op", modify_op },
           std::pair{ "moded", moded },
           std::pair{ "store", store },
           std::pair{ "cmp_a", cmp_a },
           std::pair{ "sta", sta },
           std::pair{ "alu", alu },
           std::pair{ "carry", carry },
           std::pair{ "pointer", pointer },
           std::pair{ "pointer_x", pointer_x },
           std::pair{ "pointer_y", pointer_y },
           std::pair{ "group_zero_page", group_zero_page },
           std::pair{ "group_absolute", group_absolute },
           std::pair{ "group_index_x", group_index_x },
           std::pair{ "group_index_y", group_index_y },
           std::pair{ "ldx", ldx },
           std::pair{ "ldy", ldy },
           std::pair{ "cpx", cpx },
           std::pair{ "cpy", cpy },
           std::pair{ "stx", stx },
           std::pair{ "sty", sty },
           std::pair{ "bit_op", bit_op },
           std::pair{ "indirect", indirect },
           std::pair{ "zero_page_indexed", zero_page_indexed },
           std::pair{ "indexed_absolute", indexed_absolute },
           std::pair{ "zero_page", zero_page },
           std::pair{ "absolute", absolute },
           std::pair{ "direct", direct },
           std::pair{ "addressed", addressed },
           std::pair{ "compare", compare },
           std::pair{ "inx", inx },
           std::pair{ "iny", iny },
           std::pair{ "dex", dex },
           std::pair{ "dey", dey },
           std::pair{ "tsx", tsx },
           std::pair{ "tax", tax },
           std::pair{ "tay", tay },
           std::pair{ "txa", txa },
           std::pair{ "tya", tya },
           std::pair{ "to_a", to_a },
           std::pair{ "implied_index", implied_index },
           std::pair{ "index_load", index_load },
           std::pair{ "index_op", index_op },
           std::pair{ "asl", asl },
           std::pair{ "rol", rol },
           std::pair{ "lsr", lsr },
           std::pair{ "ror", ror },
           std::pair{ "shift", shift },
           std::pair{ "shift_left", shift_left },
           std::pair{ "shift_in_zero", shift_in_zero },
           std::pair{ "jmp", jmp },
           std::pair{ "branch", branch },
           std::pair{ "clc", clc },
           std::pair{ "sec", sec },
           std::pair{ "clv", clv },
           std::pair{ "cli", cli },
           std::pair{ "sei", sei },
           std::pair{ "cld", cld },
           std::pair{ "sed", sed },
           std::pair{ "set_c", set_c },
           std::pair{ "set_i", set_i },
           std::pair{ "set_d", set_d },
           std::pair{ "pha", pha },
           std::pair{ "pla", pla },
           std::pair{ "php", php },
           std::pair{ "plp", plp },
           std::pair{ "jsr", jsr },
           std::pair{ "rts", rts },
           std::pair{ "brk", brk },
           std::pair{ "rti", rti },
           std::pair{ "txs", txs },
           std::pair{ "nop", nop },
           std::pair{ "push_op", push_op },
           std::pair{ "pull_op", pull_op },
           std::pair{ "pulls_p", pulls_p },
           std::pair{ "subroutine", subroutine },
           std::pair{ "stack_op", stack_op },
           std::pair{ "implied", implied },
           std::pair{ "tested", tested },
           std::pair{ "taken", taken },
           std::pair{ "through_pointer", through_pointer },
           std::pair{ "reading_pointer", reading_pointer },
           std::pair{ "index_x", index_x },
           std::pair{ "index_y", index_y },
           std::pair{ "page_crossed", page_crossed },
           std::pair{ "t0", t0 },
           std::pair{ "modifying", modifying },
           std::pair{ "writing_back", writing_back },
           std::pair{ "one_more", one_more },
           std::pair{ "last", last },
           std::pair{ "modified", modified },
           std::pair{ "nmi_before", nmi_before },
           std::pair{ "nmi_waiting", nmi_waiting },
           std::pair{ "nmi_request", nmi_request },
           std::pair{ "request", request },
           std::pair{ "interrupting", interrupting },
           std::pair{ "hardware", hardware },
           std::pair{ "to_nmi", to_nmi },
           std::pair{ "operand", operand },
           std::pair{ "execute", execute },
           std::pair{ "shifted", shifted },
           std::pair{ "moved", moved },
           std::pair{ "pull_a", pull_a },
           std::pair{ "pulled_p", pulled_p },
           std::pair{ "compared", compared },
           std::pair{ "push_high", push_high },
           std::pair{ "push_low", push_low },
           std::pair{ "push_p", push_p },
           std::pair{ "pushed", pushed },
           std::pair{ "vector", vector },
           std::pair{ "load_adl", load_adl },
           std::pair{ "load_adh", load_adh },
           std::pair{ "load_dl", load_dl },
           std::pair{ "jump", jump },
           std::pair{ "stores", stores },
           std::pair{ "write", write },
           std::pair{ "fetched", fetched },
           std::pair{ "from_address", from_address },
           std::pair{ "from_dl", from_dl },
           std::pair{ "from_memory", from_memory },
           std::pair{ "from_adder", from_adder },
           std::pair{ "from_data", from_data },
           std::pair{ "index_execute", index_execute },
           std::pair{ "from_a", from_a },
           std::pair{ "from_bus", from_bus },
           std::pair{ "increment", increment },
           std::pair{ "rotate", rotate },
           std::pair{ "from_alu", from_alu },
           std::pair{ "load_cv", load_cv },
           std::pair{ "tested_bits", tested_bits },
           std::pair{ "in_stack", in_stack },
           std::pair{ "effective", effective },
           std::pair{ "effective_high", effective_high },
           std::pair{ "pointer_high", pointer_high },
           std::pair{ "fetch", fetch },
           std::pair{ "push_a", push_a } })
        n.name(w, name);

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

    constexpr std::array<const char*, 6> names{ "c", "v", "n", "z", "i", "d" };
    for (int i = 0; i < 6; ++i)
        output(names[i], bit(f, flags, i));
    output("sync", t0);

    n.place();

    return ret;
}

bench make_bench(irt::component_access& ids, const circuit& tested) noexcept
{
    bench ret{ .id      = ids.alloc_generic_component(),
               .inputs  = irt::vector<irt::child_id>(tested.x.ssize(),
                                                     irt::reserve_tag),
               .outputs = irt::vector<irt::child_id>(tested.y.ssize(),
                                                     irt::reserve_tag) };

    const auto& c     = ids.components[ret.id];
    auto&       gen   = ids.generic_components.get(c.id.generic_id);
    const auto& ports = ids.components[tested.id];
    netlist     n(c, gen);

    const auto dut = n.child(tested.id, "circuit");

    for (const auto x : tested.x) {
        const auto cst = n.gate(dynamics_type::constant, {});
        n.link(cst, dut, compo(x));
        n.name(cst, irt::format_n<31>("x{}-{}", ret.inputs.ssize(),
                                      ports.x.get<irt::port_str>(x).sv())
                      .sv());
        ret.inputs.emplace_back(*cst.child);
    }

    if (tested.clk) {
        const auto clk = n.gate(dynamics_type::logical_clock, {});
        gen.children_parameters[*clk.child].set_logical_clock(clock_period,
                                                              clock_period / 2);
        n.name(clk, "clk");
        n.link(clk, dut, compo(*tested.clk));
    }

    for (const auto y : tested.y) {
        const auto cnt = n.gate(dynamics_type::counter,
                                { netlist::output_of(dut, y) });
        n.name(cnt, irt::format_n<31>("y{}-{}", ret.outputs.ssize(),
                                      ports.y.get<irt::port_str>(y).sv())
                      .sv());
        ret.outputs.emplace_back(*cnt.child);
    }

    n.place();

    return ret;
}

irt::dir_path_id make_circuits_dir(irt::modeling&         mod,
                                   const std::string_view dir) noexcept
{
    return mod.files.write([&](auto& fs) {
        const auto reg                   = fs.alloc_registred("circuits", 0);
        fs.registred_paths.get(reg).path = dir;

        const auto id               = fs.alloc_dir(reg);
        fs.dir_paths.get(id).parent = reg;
        fs.dir_paths.get(id).path   = "digital";

        (void)fs.create_directories(reg);
        (void)fs.create_directories(id);
        return id;
    });
}

irt::status write_program_file(const std::filesystem::path&   file,
                               const std::span<const irt::u8> bytes) noexcept
{
    std::ofstream ofs(file, std::ios::binary);
    const auto    count = std::max(
      std::ssize(bytes), std::ptrdiff_t{ irt::external_source_chunk_size });

    for (std::ptrdiff_t i = 0; i < count; ++i) {
        const double v = i < std::ssize(bytes)
                           ? bytes[static_cast<std::size_t>(i)]
                           : 0.0;
        ofs.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

    return irt::fail_if(not ofs.good(), std::errc::io_error);
}

irt::status save_circuits(
  irt::modeling&                                                   mod,
  const irt::dir_path_id                                           dir_id,
  std::initializer_list<std::pair<irt::component_id, const char*>> files)
{
    for (const auto& [compo_id, name] : files) {
        const auto file_id = mod.files.write([&](auto& fs) {
            return fs.alloc_file(dir_id, name, irt::file_type::component_file);
        });

        const auto saved = mod.ids.write([&](auto& ids) {
            const std::string_view file   = name;
            ids.components[compo_id].name = file.substr(0, file.rfind('.'));
            ids.component_file_paths[compo_id].file = file_id;
            return mod.files.read([&](const auto& fs, auto) {
                return mod.save(ids, fs, compo_id);
            });
        });

        if (not saved)
            return saved.error();
    }

    return irt::success();
}

irt::status save_circuits(
  irt::modeling&                                                   mod,
  const std::string_view                                           dir,
  std::initializer_list<std::pair<irt::component_id, const char*>> files)
{
    return save_circuits(mod, make_circuits_dir(mod, dir), files);
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
                      .comparator  = as_circuit(adder_8),
                      .alu         = alu };
}

bench make_cpu_bench(irt::component_access&   ids,
                     std::span<const irt::u8> program) noexcept
{
    return make_bench(ids, make_cpu(ids, make_cpu_parts(ids, program)));
}

namespace {

constexpr std::array<irt::u8, 0x3e> fibonacci_program = [] {
    std::array<irt::u8, 0x3e> bytes{};
    const auto put = [&](const int address, std::initializer_list<irt::u8> b) {
        std::copy(b.begin(), b.end(), bytes.begin() + address);
    };

    put(0x0000, { 0xa2, 0xff });       // LDX #$FF
    put(0x0002, { 0x9a });             // TXS
    put(0x0003, { 0xf8 });             // SED
    put(0x0004, { 0xa9, 0x00 });       // LDA #$00
    put(0x0006, { 0x85, 0x10 });       // STA $10
    put(0x0008, { 0xa9, 0x03 });       // LDA #$03
    put(0x000a, { 0x85, 0x11 });       // STA $11: ($10) = $0300
    put(0x000c, { 0xa9, 0x00 });       // LDA #$00
    put(0x000e, { 0x85, 0x12 });       // STA $12
    put(0x0010, { 0xa9, 0x01 });       // LDA #$01
    put(0x0012, { 0x85, 0x13 });       // STA $13
    put(0x0014, { 0xa0, 0x00 });       // LDY #$00
    put(0x0016, { 0x20, 0x30, 0x00 }); // JSR $0030
    put(0x0019, { 0xb0, 0xe9 });       // BCS $0004: beyond 99
    put(0x001b, { 0x91, 0x10 });       // STA ($10),Y
    put(0x001d, { 0xc8 });             // INY
    put(0x001e, { 0xe8 });             // INX
    put(0x001f, { 0x4c, 0x16, 0x00 }); // JMP $0016
    put(0x0030, { 0x18 });             // CLC
    put(0x0031, { 0xa5, 0x12 });       // LDA $12
    put(0x0033, { 0x65, 0x13 });       // ADC $13: the next number
    put(0x0035, { 0x48 });             // PHA
    put(0x0036, { 0xa5, 0x13 });       // LDA $13
    put(0x0038, { 0x85, 0x12 });       // STA $12
    put(0x003a, { 0x68 });             // PLA
    put(0x003b, { 0x85, 0x13 });       // STA $13
    put(0x003d, { 0x60 });             // RTS

    return bytes;
}();

} // namespace

std::span<const irt::u8> demo_program() noexcept { return fibonacci_program; }

irt::status generate(const std::string_view dir) noexcept
{
    irt::modeling mod;
    const auto    dir_id = make_circuits_dir(mod, dir);

    irt_check(write_program_file(std::filesystem::path(dir) / "digital" /
                                   demo_program_file,
                                 demo_program()));
    const auto file = mod.files.write([&](auto& fs) {
        return fs.alloc_file(dir_id, demo_program_file,
                             irt::file_type::binary_file);
    });

    const auto compo = mod.ids.write([&](auto& ids) {
        const auto      fa      = make_full_adder(ids);
        const auto      adder_8 = make_adder<8>(ids, fa);
        const auto      alu     = make_alu(ids, adder_8);
        const auto      alu_b   = make_bench(ids, alu);
        const auto      reg     = make_register<8>(ids);
        const cpu_parts parts{ .pc     = make_program_counter(ids),
                               .memory = make_memory(ids, demo_program(), file),
                               .accumulator = make_accumulator(ids, alu, reg),
                               .flags       = make_flags(ids),
                               .index       = make_index_registers(ids, reg),
                               .stack       = make_stack_pointer(ids, reg),
                               .reg         = reg,
                               .adder = as_circuit(make_adder<16>(ids, fa)),
                               .comparator = as_circuit(adder_8),
                               .alu        = alu };
        const auto      cpu   = make_cpu(ids, parts);
        const auto      cpu_b = make_bench(ids, cpu);

        return std::array{ fa.id,
                           adder_8.id,
                           parts.adder.id,
                           alu.id,
                           alu_b.id,
                           reg.id,
                           parts.accumulator.id,
                           parts.flags.id,
                           parts.index.id,
                           parts.stack.id,
                           parts.pc.id,
                           parts.memory.id,
                           cpu.id,
                           cpu_b.id };
    });

    return save_circuits(mod, dir_id,
                         { { compo[0], "full-adder.irt" },
                           { compo[1], "adder-8.irt" },
                           { compo[2], "adder-16.irt" },
                           { compo[3], "alu-8.irt" },
                           { compo[4], "alu-bench.irt" },
                           { compo[5], "register-8.irt" },
                           { compo[6], "accumulator.irt" },
                           { compo[7], "flags.irt" },
                           { compo[8], "index-registers.irt" },
                           { compo[9], "stack-pointer.irt" },
                           { compo[10], "program-counter.irt" },
                           { compo[11], "cpu-demo-memory.irt" },
                           { compo[12], "cpu-demo.irt" },
                           { compo[13], "cpu-demo-bench.irt" } });
}

} // namespace irt::digital
