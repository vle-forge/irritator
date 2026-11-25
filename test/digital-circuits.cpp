// Copyright (c) 2026 INRA Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// Digital circuits built from the logical models and nested components: the
// expected results are exact, and the size of the circuits grows quickly.

#include <irritator/core.hpp>
#include <irritator/format.hpp>
#include <irritator/io.hpp>
#include <irritator/modeling.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <utility>

#include <boost/ut.hpp>

#include "digital.hpp"
#include "utils.hpp"

namespace {

using irt::connection;
using irt::dynamics_type;
using namespace irt::digital;

/** The results of the 6502 operations on a, b and the carry: SBC is an ADC
 * of the complement of b, and the carry of the logical operations is kept.
 * In decimal mode, ADC and SBC correct each digit of the result as the NMOS
 * 6502 does, even for digits above 9: SBC keeps the flags of the binary
 * subtraction, ADC its Z flag but takes N and V from the high digit before
 * its correction. */
struct alu_result {
    unsigned r, c, v, n, z;
};

constexpr alu_result alu_reference(const alu_op   op,
                                   const unsigned a,
                                   const unsigned b,
                                   const unsigned cin,
                                   const unsigned decimal = 0) noexcept
{
    const auto result = [](const unsigned r, const unsigned c,
                           const unsigned v) {
        return alu_result{ r, c, v, r >> 7, r == 0 ? 1u : 0u };
    };

    const auto add = [&](const unsigned x, const unsigned y, const unsigned c) {
        const auto sum = x + y + c;
        const auto r   = sum & 0xffu;
        const auto v   = ((x ^ r) & (y ^ r) & 0x80u) != 0 ? 1u : 0u;
        return result(r, sum >> 8, v);
    };

    const auto decimal_adc = [&]() {
        auto low = (a & 0x0fu) + (b & 0x0fu) + cin;
        if (low >= 0x0au)
            low = ((low + 0x06u) & 0x0fu) + 0x10u;

        auto       sum  = (a & 0xf0u) + (b & 0xf0u) + low;
        const auto high = sum & 0xffu;
        const auto v    = ((a ^ high) & (b ^ high) & 0x80u) != 0 ? 1u : 0u;
        if (sum >= 0xa0u)
            sum += 0x60u;

        return alu_result{ sum & 0xffu, sum >= 0x100u ? 1u : 0u, v, high >> 7,
                           add(a, b, cin).z };
    };

    const auto decimal_sbc = [&]() {
        auto binary = add(a, ~b & 0xffu, cin);
        auto low = static_cast<int>(a & 0x0fu) - static_cast<int>(b & 0x0fu) +
                   static_cast<int>(cin) - 1;
        if (low < 0)
            low = ((low - 0x06) & 0x0f) - 0x10;

        auto diff = static_cast<int>(a & 0xf0u) - static_cast<int>(b & 0xf0u) +
                    low;
        if (diff < 0)
            diff -= 0x60;

        binary.r = static_cast<unsigned>(diff) & 0xffu;
        return binary;
    };

    switch (op) {
    case alu_op::ora:
        return result(a | b, cin, 0);
    case alu_op::and_:
        return result(a & b, cin, 0);
    case alu_op::eor:
        return result(a ^ b, cin, 0);
    case alu_op::adc:
        return decimal ? decimal_adc() : add(a, b, cin);
    case alu_op::sbc:
        return decimal ? decimal_sbc() : add(a, ~b & 0xffu, cin);
    case alu_op::rol:
        return result(((a << 1) | cin) & 0xffu, a >> 7, 0);
    case alu_op::ror:
        return result((a >> 1) | (cin << 7), a & 1u, 0);
    case alu_op::pass_b:
        return result(b, cin, 0);
    }

    return {};
}

static_assert(alu_reference(alu_op::adc, 0x50, 0x50, 0).v == 1);
static_assert(alu_reference(alu_op::sbc, 0x50, 0x10, 1).r == 0x40);
static_assert(alu_reference(alu_op::sbc, 0x50, 0x10, 1).c == 1);
static_assert(alu_reference(alu_op::adc, 0x58, 0x46, 1, 1).r == 0x05);
static_assert(alu_reference(alu_op::adc, 0x58, 0x46, 1, 1).c == 1);
static_assert(alu_reference(alu_op::adc, 0x99, 0x01, 0, 1).z == 0);
static_assert(alu_reference(alu_op::sbc, 0x12, 0x21, 1, 1).r == 0x91);
static_assert(alu_reference(alu_op::sbc, 0x12, 0x21, 1, 1).c == 0);

/** Inputs: a0..a7, b0..b7, cin, op0..op2 then d. Outputs: r0..r7, c, v, n
 * and z. */
constexpr irt::u64 alu_inputs(const alu_op   op,
                              const unsigned a,
                              const unsigned b,
                              const unsigned cin,
                              const unsigned decimal = 0) noexcept
{
    return a | (b << 8) | (cin << 16) | (static_cast<irt::u64>(op) << 17) |
           (irt::u64{ decimal } << 20);
}

constexpr irt::u64 alu_outputs(const alu_result& res) noexcept
{
    return res.r | (res.c << 8) | (res.v << 9) | (res.n << 10) | (res.z << 11);
}

/** The registers of the 6502 seen from the outputs of the CPU. */
struct cpu_state {
    unsigned pc = 0, a = 0, x = 0, y = 0, s = 0, c = 0, v = 0, n = 0, z = 0,
             i = 0, d = 0;
};

/** The status register P pushed by PHP and BRK, with the bits 5 and 4 (B)
 * set. */
constexpr unsigned status_of(const cpu_state& s) noexcept
{
    return s.c | (s.z << 1) | (s.i << 2) | (s.d << 3) | 0x30u | (s.v << 6) |
           (s.n << 7);
}

/** The vectors of NMI then of IRQ and BRK. */
constexpr unsigned nmi_vector = 0xfffa, break_vector = 0xfffe;

/** Pushes the return address and P, sets I and jumps through the vector, as
 * BRK and the interrupts do. */
constexpr void enter_interrupt(cpu_state&         s,
                               std::span<irt::u8> mem,
                               const unsigned     address,
                               const unsigned     status,
                               const unsigned     vector) noexcept
{
    for (const auto value : { address >> 8, address & 0xffu, status }) {
        mem[0x100u + s.s] = static_cast<irt::u8>(value);
        s.s               = (s.s - 1u) & 0xffu;
    }

    s.i  = 1;
    s.pc = mem[vector] | (unsigned{ mem[vector + 1] } << 8);
}

/** Answers an interrupt request instead of the instruction at pc, in 7
 * cycles: P is pushed without B, pc is the return address. */
constexpr int cpu_interrupt(cpu_state&         s,
                            std::span<irt::u8> mem,
                            const bool         nmi) noexcept
{
    enter_interrupt(s, mem, s.pc, status_of(s) & ~0x10u,
                    nmi ? nmi_vector : break_vector);
    return 7;
}

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

    const auto pull_status = [&]() {
        const auto p = pull();
        s.c          = p & 1u;
        s.z          = (p >> 1) & 1u;
        s.i          = (p >> 2) & 1u;
        s.d          = (p >> 3) & 1u;
        s.v          = (p >> 6) & 1u;
        s.n          = p >> 7;
    };

    // The pointers of the zero page wrap in the zero page.
    const auto pointer = [&](const unsigned at) -> unsigned {
        return mem[at & 0xffu] | (unsigned{ mem[(at + 1u) & 0xffu] } << 8);
    };

    if (has_modes(op)) {
        const auto mode  = mode_of(op);
        const auto base  = static_cast<unsigned>(op) & 0xe3u;
        const auto index = indexes_with_y(op) ? s.y : s.x;
        const auto start = mode == addressing::indirect_y ? pointer(byte(1))
                                                          : address;
        const auto ea    = [&]() -> unsigned {
            switch (mode) {
            case addressing::indirect_x:
                return pointer(byte(1) + s.x);
            case addressing::zero_page:
                return byte(1);
            case addressing::zero_page_x:
                return (byte(1) + index) & 0xffu;
            case addressing::absolute_x:
            case addressing::absolute_y:
            case addressing::indirect_y:
                return (start + index) & 0xffffu;
            default:
                return address;
            }
        }();
        const auto crossed = ((ea ^ start) & 0xff00u) != 0;
        const auto operand = mode == addressing::immediate
                               ? byte(1)
                               : unsigned{ mem[ea] };
        s.pc               = next;

        // The indexed modes of 16 bits addresses take one more cycle to
        // cross a page, the stores always.
        const auto indexed = mode == addressing::absolute_x or
                             mode == addressing::absolute_y or
                             mode == addressing::indirect_y;
        const auto cycles  = mode == addressing::immediate     ? 2
                             : mode == addressing::zero_page   ? 3
                             : mode == addressing::zero_page_x ? 4
                             : mode == addressing::indirect_x  ? 6
                             : mode == addressing::indirect_y  ? 5
                                                               : 4;
        const auto extra   = indexed and (crossed or is_store(op)) ? 1 : 0;

        if (is_store(op)) {
            const auto stx = base == 0x82u;
            const auto sty = base == 0x80u;
            mem[ea]        = static_cast<irt::u8>(stx ? s.x : sty ? s.y : s.a);
            return cycles + extra;
        }

        if (is_bit(op)) {
            s.n = operand >> 7;
            s.v = (operand >> 6) & 1u;
            s.z = (s.a & operand) == 0 ? 1u : 0u;
            return cycles;
        }

        if (is_read_modify_write(op)) {
            const auto kind  = static_cast<unsigned>(op) >> 5;
            auto       value = operand;
            if (kind == 0b110u) {
                value = (operand - 1u) & 0xffu;
            } else if (kind == 0b111u) {
                value = (operand + 1u) & 0xffu;
            } else {
                const auto res = alu_reference(
                  kind < 2 ? alu_op::rol : alu_op::ror, operand, 0,
                  kind & 1u ? s.c : 0u);
                value = res.r;
                s.c   = res.c;
            }
            mem[ea] = static_cast<irt::u8>(set_nz(value));
            return mode == addressing::zero_page    ? 5
                   : mode == addressing::absolute_x ? 7
                                                    : 6;
        }

        switch (base) {
        case 0xc1u:
            compare(s.a, operand);
            return cycles + extra;
        case 0xa2u:
            s.x = set_nz(operand);
            return cycles + extra;
        case 0xa0u:
            s.y = set_nz(operand);
            return cycles + extra;
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
        const auto res = alu_reference(alu, s.a, operand, s.c, s.d);
        s.a            = res.r;
        s.n            = res.n;
        s.z            = res.z;
        if (alu == alu_op::adc or alu == alu_op::sbc) {
            s.c = res.c;
            s.v = res.v;
        }

        return cycles + extra;
    }

    if (is_branch(op)) {
        const std::array flags{ s.n, s.v, s.c, s.z };
        const auto       code = static_cast<unsigned>(op);
        const auto offset     = static_cast<int>(static_cast<irt::i8>(byte(1)));
        s.pc                  = next;
        if (flags[code >> 6] != ((code >> 5) & 1u))
            return 2;

        const auto target = static_cast<unsigned>(static_cast<int>(next) +
                                                  offset) &
                            0xffffu;
        s.pc              = target;
        return ((target ^ next) & 0xff00u) != 0 ? 4 : 3;
    }

    switch (op) {
    case opcode::jmp_abs:
        s.pc = address;
        return 3;

    case opcode::jmp_ind: {
        // The 6502 does not carry into the high byte of the pointer.
        const auto high = (address & 0xff00u) | ((address + 1u) & 0x00ffu);
        s.pc            = mem[address] | (unsigned{ mem[high] } << 8);
        return 5;
    }

    case opcode::clc:
        s.c = 0;
        break;
    case opcode::sec:
        s.c = 1;
        break;
    case opcode::clv:
        s.v = 0;
        break;
    case opcode::cli:
        s.i = 0;
        break;
    case opcode::sei:
        s.i = 1;
        break;
    case opcode::cld:
        s.d = 0;
        break;
    case opcode::sed:
        s.d = 1;
        break;

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

    case opcode::brk:
        enter_interrupt(s, mem, next, status_of(s), break_vector);
        return 7;

    case opcode::rti: {
        pull_status();
        const auto low = pull();
        s.pc           = low | (pull() << 8);
        return 6;
    }

    case opcode::pha:
        push(s.a);
        s.pc = next;
        return 3;

    case opcode::php:
        push(status_of(s));
        s.pc = next;
        return 3;

    case opcode::pla:
        s.a  = set_nz(pull());
        s.pc = next;
        return 4;

    case opcode::plp:
        pull_status();
        s.pc = next;
        return 4;

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
    case opcode::tax:
        s.x = set_nz(s.a);
        break;
    case opcode::tay:
        s.y = set_nz(s.a);
        break;
    case opcode::txa:
        s.a = set_nz(s.x);
        break;
    case opcode::tya:
        s.a = set_nz(s.y);
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

enum class interrupt { none, irq, nmi };

/** The inputs irq and nmi of the CPU cycle by cycle, in the bits 0 and 1,
 * false beyond the end. The CPU polls them on the rising edge of the fetch
 * cycle of each opcode: nmi when it rose since the previous fetch, irq when
 * it is true and I is not set. */
class interrupt_lines
{
public:
    explicit interrupt_lines(std::span<const irt::u8> lines) noexcept
      : m_lines(lines)
    {}

    unsigned at(const int cycle) const noexcept
    {
        return cycle <= std::ssize(m_lines)
                 ? m_lines[static_cast<std::size_t>(cycle - 1)]
                 : 0u;
    }

    struct step_result {
        int       cycles;
        interrupt kind;
    };

    /** Runs the interrupt or the instruction whose fetch is the cycle @a
     * cycle, the first one is 1. */
    step_result step(cpu_state&         s,
                     std::span<irt::u8> mem,
                     const int          cycle) noexcept
    {
        const auto nmi = nmi_edge(cycle) or m_nmi_waiting;
        const auto irq = (at(cycle) & 1u) != 0 and s.i == 0;
        m_nmi_waiting  = false;

        const auto kind     = nmi   ? interrupt::nmi
                              : irq ? interrupt::irq
                                    : interrupt::none;
        const auto duration = kind == interrupt::none
                                ? cpu_reference(s, mem)
                                : cpu_interrupt(s, mem, nmi);
        for (int k = cycle + 1; k < cycle + duration; ++k)
            m_nmi_waiting = nmi_edge(k) or m_nmi_waiting;

        return { duration, kind };
    }

private:
    bool nmi_edge(const int cycle) noexcept
    {
        const auto nmi  = (at(cycle) >> 1) & 1u;
        const auto edge = nmi == 1u and m_nmi_before == 0u;
        m_nmi_before    = nmi;
        return edge;
    }

    std::span<const irt::u8> m_lines;
    unsigned                 m_nmi_before  = 0;
    bool                     m_nmi_waiting = false;
};

/** The position of sync in the outputs of the CPU. */
constexpr int cpu_sync = 54;

/** The outputs of the CPU: pc0..pc15, a0..a7, x0..x7, y0..y7, s0..s7, c, v,
 * n, z, i, d then sync. */
constexpr irt::u64 cpu_outputs(const cpu_state& s, const unsigned sync) noexcept
{
    return s.pc | (irt::u64{ s.a } << 16) | (irt::u64{ s.x } << 24) |
           (irt::u64{ s.y } << 32) | (irt::u64{ s.s } << 40) |
           (irt::u64{ s.c } << 48) | (irt::u64{ s.v } << 49) |
           (irt::u64{ s.n } << 50) | (irt::u64{ s.z } << 51) |
           (irt::u64{ s.i } << 52) | (irt::u64{ s.d } << 53) |
           (irt::u64{ sync } << cpu_sync);
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

        for (irt::ssz i = 0, e = m_bench.inputs.ssize(); i != e; ++i)
            m_pj.sim.parameters[head.children[m_bench.inputs[i]].mdl]
              .set_constant(static_cast<irt::real>((inputs >> i) & 1u));

        m_cycle = 0;
        m_pj.sim.limits.set_bound(0, (cycles + 1) * clock_period);
        boost::ut::expect(
          boost::ut::fatal(m_pj.sim.srcs.prepare().has_value()));
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

        // The children are named x0-a0, x1-a1... y0-q0... after the ports
        // they feed or read.
        const auto find = [&](const char prefix, const int i) {
            const auto name = irt::format_n<8>("{}{}-", prefix, i);
            for (const auto& child : gen.children) {
                const auto id = gen.children.get_id(child);
                if (gen.children_names[id].sv().starts_with(name.sv()))
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

/** The branches taken during a run, some of them cross a page. */
struct cpu_run {
    int taken_branches = 0;
    int crossed_pages  = 0;

    /** The reads at an absolute address plus X or Y in the next page. */
    int indexed_crossings = 0, y_crossings = 0;

    /** The zero page addresses plus X which wrap in the zero page. */
    int zero_page_wraps = 0;

    int calls = 0, read_modify_writes = 0, indirect_jumps = 0;

    /** The operands read or written through a pointer of the zero page, and
     * those of ($zp),Y in the next page. */
    int pointer_operands = 0, pointer_crossings = 0;

    int decimal_operations = 0, breaks = 0;

    /** The interrupts answered, those which push P with I set: an NMI in
     * the handler of an interrupt or of BRK. */
    int irqs = 0, nmis = 0, nested_interrupts = 0;

    /** The comparisons of a register greater, equal or lower than the
     * operand. */
    int compares_greater = 0, compares_equal = 0, compares_lower = 0;

    /** The lowest address written by a push, in the page 0x0100. */
    unsigned lowest_push = 0xff;
};

/** Runs @a instructions instructions of the @a program on the CPU and on the
 * reference, cycle by cycle: sync is true only on the first cycle of an
 * instruction, when the registers show the results of the previous ones. The
 * inputs irq and nmi follow the @a lines, see interrupt_lines, an answered
 * interrupt counts as an instruction. */
cpu_run check_cpu(bench_simulation&        sim,
                  std::span<const irt::u8> program,
                  const int                instructions,
                  std::span<const irt::u8> lines = {}) noexcept
{
    cpu_run              run;
    irt::vector<irt::u8> mem(irt::memory_bank::size, irt::u8{ 0 });
    std::copy(program.begin(), program.end(), mem.begin());
    const auto bytes = std::span(mem.data(), mem.size());

    auto            dry_run = mem;
    int             cycles  = 0;
    interrupt_lines dry_lines(lines);
    for (cpu_state s; const auto i : std::views::iota(0, instructions)) {
        (void)i;
        cycles += dry_lines
                    .step(s, std::span(dry_run.data(), dry_run.size()),
                          cycles + 1)
                    .cycles;
    }

    cpu_state       ref;
    interrupt_lines reference(lines);
    unsigned        inputs = 0;
    sim.start(0, cycles);
    for (int i = 0, k = 0; i < instructions; ++i) {
        const auto pc    = ref.pc;
        const auto op    = static_cast<opcode>(bytes[pc]);
        const auto mode  = has_modes(op) ? mode_of(op) : addressing::immediate;
        const auto wraps = bytes[(pc + 1) & 0xffffu] +
                             (indexes_with_y(op) ? ref.y : ref.x) >
                           0xffu;
        const auto decimal          = is_decimal_operation(op) and ref.d;
        const auto masked           = ref.i;
        const auto [duration, kind] = reference.step(ref, bytes, k + 1);

        if (kind != interrupt::none) {
            ++(kind == interrupt::irq ? run.irqs : run.nmis);
            if (masked)
                ++run.nested_interrupts;
            run.lowest_push = std::min(run.lowest_push, (ref.s + 1u) & 0xffu);
        } else {
            if (is_branch(op) and duration > 2)
                ++run.taken_branches;
            if (is_branch(op) and duration > 3)
                ++run.crossed_pages;
            if ((mode == addressing::absolute_x or
                 mode == addressing::absolute_y) and
                not writes_at_address(op) and duration > 4) {
                ++run.indexed_crossings;
                if (mode == addressing::absolute_y)
                    ++run.y_crossings;
            }
            if (mode == addressing::zero_page_x and wraps)
                ++run.zero_page_wraps;
            if (op == opcode::jsr_abs)
                ++run.calls;
            if (is_read_modify_write(op))
                ++run.read_modify_writes;
            if (op == opcode::jmp_ind)
                ++run.indirect_jumps;
            if (has_modes(op) and (mode == addressing::indirect_x or
                                   mode == addressing::indirect_y))
                ++run.pointer_operands;
            if (has_modes(op) and mode == addressing::indirect_y and
                not is_store(op) and duration > 5)
                ++run.pointer_crossings;
            if (decimal)
                ++run.decimal_operations;
            if (op == opcode::brk)
                ++run.breaks;
            if (op == opcode::pha or op == opcode::php or
                op == opcode::jsr_abs or op == opcode::brk)
                run.lowest_push = std::min(run.lowest_push,
                                           (ref.s + 1u) & 0xffu);
            if (is_compare(op)) {
                if (ref.z)
                    ++run.compares_equal;
                else if (ref.c)
                    ++run.compares_greater;
                else
                    ++run.compares_lower;
            }
        }

        for (int j = 1; j <= duration; ++j) {
            if (reference.at(k + 1) != inputs) {
                inputs = reference.at(k + 1);
                sim.set_inputs(inputs);
            }

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
                           const unsigned cin, const unsigned decimal) {
        const auto expected = alu_outputs(
          alu_reference(op, x, y, cin, decimal));
        boost::ut::expect(boost::ut::eq(
          sim.evaluate(alu_inputs(op, x, y, cin, decimal)), expected))
          << "op " << static_cast<unsigned>(op) << " a " << x << " b " << y
          << " cin " << cin << " d " << decimal;
    };

    for (unsigned o = 0; o < alu_op_count; ++o) {
        const auto op = static_cast<alu_op>(o);
        for (unsigned cin = 0; cin < 2; ++cin) {
            for (const auto x : edges)
                for (const auto y : edges)
                    check(op, x, y, cin, 0);

            for (int i = 0; i < 64; ++i)
                check(op, gen() & 0xffu, gen() & 0xffu, cin, 0);
        }
    }

    // The digits above 9 of the decimal mode, and the decimal mode ignored
    // by the other operations.
    constexpr std::array<unsigned, 8> digits{ 0x00, 0x09, 0x0a, 0x0f,
                                              0x90, 0x99, 0xa0, 0xff };
    for (const auto op : { alu_op::adc, alu_op::sbc })
        for (unsigned cin = 0; cin < 2; ++cin) {
            for (const auto x : digits)
                for (const auto y : digits)
                    check(op, x, y, cin, 1);

            for (int i = 0; i < 128; ++i)
                check(op, gen() & 0xffu, gen() & 0xffu, cin, 1);
        }

    for (unsigned o = 0; o < alu_op_count; ++o)
        for (int i = 0; i < 8; ++i)
            check(static_cast<alu_op>(o), gen() & 0xffu, gen() & 0xffu,
                  gen() & 1u, 1);
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
            const auto op      = static_cast<alu_op>(gen() % alu_op_count);
            const auto x       = gen() & 0xffu;
            const auto cin     = gen() & 1u;
            const auto load    = gen() & 1u;
            const auto decimal = gen() & 1u;

            sim.set_inputs(x | (cin << 8) | (static_cast<irt::u64>(op) << 9) |
                           (static_cast<irt::u64>(load) << 12) |
                           (static_cast<irt::u64>(decimal) << 13));
            if (load)
                a = alu_reference(op, a, x, cin, decimal).r;

            // The flags are combinational: the ALU result for the new A.
            const auto flags = alu_outputs(
                                 alu_reference(op, a, x, cin, decimal)) >>
                               8;
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
        const auto  temp = irt::temp_path_with_unlink{ "memory-content" };
        expect(fatal(temp.success()));
        const std::string_view dir = temp.c_str();

        const auto content = random_bytes(200, 2468u);
        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto mem = make_memory(ids, content);
                const auto b   = make_bench(ids, mem);
                return std::array{ mem.id, b.id };
            });

            expect(fatal(save_circuits(mod, dir,
                                       { { compo[0], "memory.irt" },
                                         { compo[1], "memory-bench.irt" } })
                           .has_value()));
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

    "cpu-transfers-and-stores-the-index-registers"_test = [] {
        constexpr std::array<irt::u8, 36> program{
            0xa9, 0x80,       // LDA #$80
            0xaa,             // TAX: N
            0xa8,             // TAY
            0xa9, 0x00,       // LDA #$00: Z
            0x8a,             // TXA: $80, N
            0xa2, 0x00,       // LDX #$00
            0x8a,             // TXA: Z
            0xa0, 0x7f,       // LDY #$7F
            0x98,             // TYA
            0xa2, 0x11,       // LDX #$11
            0x86, 0x40,       // STX $40, 3 cycles
            0x8e, 0x01, 0x20, // STX $2001, 4 cycles
            0x84, 0x41,       // STY $41
            0x8c, 0x02, 0x20, // STY $2002
            0xa5, 0x40,       // LDA $40: $11
            0x65, 0x41,       // ADC $41: $90
            0xaa,             // TAX
            0xa8,             // TAY
            0x4c, 0x00, 0x00, // JMP $0000
            0x00, 0x00
        };

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        check_cpu(sim, program, 60);
    };

    "cpu-modifies-bytes-in-memory"_test = [] {
        // $50,X wraps to $40 with X = $F0, $1F20,X crosses to $2010.
        constexpr std::array<irt::u8, 42> program{
            0xa2, 0xf0,       // LDX #$F0
            0xa9, 0x81,       // LDA #$81
            0x85, 0x40,       // STA $40
            0x8d, 0x10, 0x20, // STA $2010
            0x06, 0x40,       // ASL $40: $02, C, 5 cycles
            0x26, 0x40,       // ROL $40: $05
            0x4e, 0x10, 0x20, // LSR $2010: $40, C, 6 cycles
            0x6e, 0x10, 0x20, // ROR $2010: $A0, N
            0x76, 0x50,       // ROR $50,X: $40, 6 cycles
            0xfe, 0x20, 0x1f, // INC $1F20,X: $2010, 7 cycles
            0xde, 0x00, 0x20, // DEC $2000,X: $20F0, 7 cycles
            0xc6, 0x41,       // DEC $41: N
            0xe6, 0x41,       // INC $41: Z
            0x3e, 0x00, 0x20, // ROL $2000,X
            0xa5, 0x40,       // LDA $40
            0x6d, 0x10, 0x20, // ADC $2010
            0x4c, 0x00, 0x00  // JMP $0000
        };

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 50);
        expect(gt(run.read_modify_writes, 20));
    };

    "cpu-tests-the-flags-and-jumps-through-pointers"_test = [] {
        irt::vector<irt::u8> program(0x200, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                                       std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // Each taken branch skips an INX.
        put(0x0000, { 0x38 });             // SEC
        put(0x0001, { 0xb0, 0x01 });       // BCS $0004
        put(0x0003, { 0xe8 });             // INX
        put(0x0004, { 0x18 });             // CLC
        put(0x0005, { 0x90, 0x01 });       // BCC $0008
        put(0x0007, { 0xe8 });             // INX
        put(0x0008, { 0xa9, 0x40 });       // LDA #$40
        put(0x000a, { 0x69, 0x40 });       // ADC #$40: V, N
        put(0x000c, { 0x70, 0x01 });       // BVS $000F
        put(0x000e, { 0xe8 });             // INX
        put(0x000f, { 0x30, 0x01 });       // BMI $0012
        put(0x0011, { 0xe8 });             // INX
        put(0x0012, { 0xb8 });             // CLV
        put(0x0013, { 0x50, 0x01 });       // BVC $0016
        put(0x0015, { 0xe8 });             // INX
        put(0x0016, { 0xa9, 0x01 });       // LDA #$01
        put(0x0018, { 0x10, 0x01 });       // BPL $001B
        put(0x001a, { 0xe8 });             // INX
        put(0x001b, { 0x24, 0x40 });       // BIT $40: $C0, N, V, Z
        put(0x001d, { 0xb0, 0x01 });       // BCS, not taken
        put(0x001f, { 0x2c, 0x41, 0x00 }); // BIT $0041: $40, V, Z
        put(0x0022, { 0x30, 0x01 });       // BMI, not taken
        put(0x0024, { 0x50, 0x01 });       // BVC, not taken
        put(0x0026, { 0x6c, 0x42, 0x00 }); // JMP ($0042): $0030
        put(0x0030, { 0x6c, 0xff, 0x01 }); // JMP ($01FF): $0100, $01FF
        put(0x0040, { 0xc0, 0x40, 0x30, 0x00 });
        put(0x0100, { 0x01 });
        put(0x0150, { 0x4c, 0x00, 0x00 }); // JMP $0000
        put(0x01ff, { 0x50 });

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 60);
        expect(gt(run.taken_branches, 10));
        expect(gt(run.indirect_jumps, 3));
    };

    "cpu-indexes-with-y-and-reads-through-pointers"_test = [] {
        irt::vector<irt::u8> program(0x100, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                                       std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // ($40) = $20F8 crosses to $2108 with Y = $10, ($FF) takes its high
        // byte in $00: $A2C0. The zero page plus an index wraps.
        put(0x0000, { 0xa2, 0x04 });       // LDX #$04
        put(0x0002, { 0xa0, 0x10 });       // LDY #$10
        put(0x0004, { 0xa9, 0x5a });       // LDA #$5A
        put(0x0006, { 0x91, 0x40 });       // STA ($40),Y: $2108, 6 cycles
        put(0x0008, { 0x81, 0x3c });       // STA ($3C,X): $20F8, 6 cycles
        put(0x000a, { 0xa9, 0x00 });       // LDA #$00
        put(0x000c, { 0x11, 0x40 });       // ORA ($40),Y: $5A, 6 cycles
        put(0x000e, { 0x41, 0x3c });       // EOR ($3C,X): Z, 6 cycles
        put(0x0010, { 0x71, 0x42 });       // ADC ($42),Y: $00F0, 5 cycles
        put(0x0012, { 0xd1, 0x42 });       // CMP ($42),Y: Z, C
        put(0x0014, { 0xa2, 0xff });       // LDX #$FF
        put(0x0016, { 0x81, 0x00 });       // STA ($00,X): $A2C0
        put(0x0018, { 0xb1, 0xff });       // LDA ($FF),Y: $A2D0, Z
        put(0x001a, { 0xa1, 0x00 });       // LDA ($00,X): $A2C0
        put(0x001c, { 0xb6, 0x30 });       // LDX $30,Y: $40, 4 cycles
        put(0x001e, { 0xbe, 0xf8, 0x20 }); // LDX $20F8,Y: $2108, 5 cycles
        put(0x0021, { 0xb4, 0xa6 });       // LDY $A6,X: $00
        put(0x0023, { 0xbc, 0xb0, 0x20 }); // LDY $20B0,X: $210A, 5 cycles
        put(0x0026, { 0xa0, 0x60 });       // LDY #$60
        put(0x0028, { 0x96, 0xf0 });       // STX $F0,Y: $50, 4 cycles
        put(0x002a, { 0x94, 0xe0 });       // STY $E0,X: $3A, 4 cycles
        put(0x002c, { 0xea });             // NOP
        put(0x002d, { 0x4c, 0x00, 0x00 }); // JMP $0000
        put(0x0040, { 0xf8, 0x20, 0xe0, 0x00 });
        put(0x00f0, { 0x33 });
        put(0x00ff, { 0xc0 });

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 60);
        expect(gt(run.pointer_operands, 15));
        expect(gt(run.pointer_crossings, 1));
        expect(gt(run.indexed_crossings, 3));
        expect(gt(run.zero_page_wraps, 3));
    };

    "cpu-computes-in-decimal-and-pulls-the-status"_test = [] {
        // The digits above 9 are corrected as by the NMOS 6502. S starts at
        // zero: the pushes write $0100 then $01FF.
        irt::vector<irt::u8> program(0x41, irt::u8{ 0 });
        program[0x40] = 0x0f;
        const std::initializer_list<irt::u8> code{
            0xf8,            // SED
            0x18,            // CLC
            0xa9, 0x58,      // LDA #$58
            0x69, 0x46,      // ADC #$46: $04, C
            0x69, 0x09,      // ADC #$09: $14
            0x38,            // SEC
            0xe9, 0x15,      // SBC #$15: $99, N
            0xe9, 0x99,      // SBC #$99: $99
            0x69, 0x9a,      // ADC #$9A
            0xe5, 0x40,      // SBC $40: $0F
            0x08,            // PHP: D
            0x68,            // PLA
            0xd8,            // CLD
            0x69, 0x01,      // ADC #$01: binary
            0x78,            // SEI
            0x08,            // PHP: I
            0x58,            // CLI
            0xa9, 0xff,      // LDA #$FF
            0x48,            // PHA
            0x28,            // PLP: every flag
            0x69, 0x00,      // ADC #$00: decimal, $FF + 1
            0xa9, 0x00,      // LDA #$00
            0x48,            // PHA
            0x28,            // PLP: no flag
            0x4c, 0x00, 0x00 // JMP $0000
        };
        std::copy(code.begin(), code.end(), program.begin());

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 60);
        expect(gt(run.decimal_operations, 10));
    };

    "cpu-breaks-and-returns-from-interrupts"_test = [] {
        irt::vector<irt::u8> program(0x50, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                                       std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // The program writes the vector of BRK, $0040, beyond its content.
        // BRK pushes the address after its signature byte and P with B.
        put(0x0000, { 0xa2, 0xff });       // LDX #$FF
        put(0x0002, { 0x9a });             // TXS
        put(0x0003, { 0xa9, 0x40 });       // LDA #$40
        put(0x0005, { 0x8d, 0xfe, 0xff }); // STA $FFFE
        put(0x0008, { 0xa9, 0x00 });       // LDA #$00
        put(0x000a, { 0x8d, 0xff, 0xff }); // STA $FFFF
        put(0x000d, { 0xf8 });             // SED
        put(0x000e, { 0x00, 0xea });       // BRK: $0010, 7 cycles
        put(0x0010, { 0x08 });             // PHP: D without I
        put(0x0011, { 0x68 });             // PLA
        put(0x0012, { 0xd8 });             // CLD
        put(0x0013, { 0x00, 0x00 });       // BRK: $0015
        put(0x0015, { 0x4c, 0x00, 0x00 }); // JMP $0000
        put(0x0040, { 0xba });             // TSX: $FC
        put(0x0041, { 0x68 });             // PLA: P
        put(0x0042, { 0x48 });             // PHA
        put(0x0043, { 0x69, 0x01 });       // ADC #$01
        put(0x0045, { 0x08 });             // PHP: I
        put(0x0046, { 0x68 });             // PLA
        put(0x0047, { 0x40 });             // RTI: 6 cycles

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 60);
        expect(gt(run.breaks, 3));
    };

    "cpu-answers-the-interrupt-requests"_test = [] {
        irt::vector<irt::u8> program(0x80, irt::u8{ 0 });
        const auto           put = [&](const int                      address,
                                       std::initializer_list<irt::u8> bytes) {
            std::copy(bytes.begin(), bytes.end(), program.begin() + address);
        };

        // The vectors: NMI at $0050, IRQ and BRK at $0060. The handler of
        // IRQ and BRK stores the P pushed, with B for BRK only. The main loop
        // masks IRQ for a while.
        put(0x0000, { 0xa2, 0xff });       // LDX #$FF
        put(0x0002, { 0x9a });             // TXS
        put(0x0003, { 0xa9, 0x50 });       // LDA #$50
        put(0x0005, { 0x8d, 0xfa, 0xff }); // STA $FFFA
        put(0x0008, { 0xa9, 0x60 });       // LDA #$60
        put(0x000a, { 0x8d, 0xfe, 0xff }); // STA $FFFE
        put(0x000d, { 0xa9, 0x00 });       // LDA #$00
        put(0x000f, { 0x8d, 0xfb, 0xff }); // STA $FFFB
        put(0x0012, { 0x8d, 0xff, 0xff }); // STA $FFFF
        put(0x0015, { 0x58 });             // CLI
        put(0x0016, { 0xe8 });             // INX
        put(0x0017, { 0x6d, 0x00, 0x20 }); // ADC $2000
        put(0x001a, { 0x8d, 0x00, 0x20 }); // STA $2000
        put(0x001d, { 0x00, 0x00 });       // BRK
        put(0x001f, { 0x78 });             // SEI
        put(0x0020, { 0xe8, 0xe8, 0xe8 }); // INX, INX, INX
        put(0x0023, { 0x58 });             // CLI
        put(0x0024, { 0x4c, 0x16, 0x00 }); // JMP $0016
        put(0x0050, { 0xc8 });             // INY
        put(0x0051, { 0x40 });             // RTI
        put(0x0060, { 0x48 });             // PHA
        put(0x0061, { 0xba });             // TSX
        put(0x0062, { 0xbd, 0x02, 0x01 }); // LDA $0102,X: P
        put(0x0065, { 0x8d, 0x01, 0x20 }); // STA $2001
        put(0x0068, { 0x68 });             // PLA
        put(0x0069, { 0x40 });             // RTI

        // irq and nmi change at random cycles, inside the instructions too:
        // irq stays true a while, nmi rises then falls.
        irt::vector<irt::u8> lines(1200, irt::reserve_tag);
        std::mt19937         gen(6502u);
        unsigned             value = 0;
        for (int k = 0; k < 1200; ++k) {
            if (gen() % 40 == 0)
                value ^= 1u;
            if (gen() % 15 == 0)
                value ^= 2u;
            lines.emplace_back(static_cast<irt::u8>(value));
        }

        irt::modeling mod;
        const auto    b = mod.ids.write(
          [&](auto& ids) { return make_cpu_bench(ids, program); });

        bench_simulation sim(mod, b);
        const auto       run = check_cpu(sim, program, 250, lines);
        expect(gt(run.irqs, 5));
        expect(gt(run.nmis, 5));
        expect(gt(run.nested_interrupts, 0));
        expect(gt(run.breaks, 1));
    };

    "cpu-runs-random-programs"_test = [] {
        cpu_run total;

        for (const auto seed : { 2024u, 6502u, 1975u, 6510u, 1977u }) {
            std::mt19937 gen(seed);

            constexpr std::array modes{
                addressing::immediate,   addressing::zero_page,
                addressing::zero_page_x, addressing::absolute,
                addressing::absolute_x,  addressing::absolute_y,
                addressing::indirect_x,  addressing::indirect_y
            };
            constexpr std::array index_modes{ addressing::immediate,
                                              addressing::zero_page,
                                              addressing::absolute };
            constexpr std::array indexed_modes{ addressing::zero_page_x,
                                                addressing::absolute_x };
            constexpr std::array index_loads{ opcode::ldx_imm,
                                              opcode::ldy_imm };
            constexpr std::array store_modes{ addressing::zero_page,
                                              addressing::absolute,
                                              addressing::absolute_x,
                                              addressing::absolute_y,
                                              addressing::indirect_y };
            constexpr std::array modify_modes{ addressing::zero_page,
                                               addressing::absolute,
                                               addressing::absolute_x };
            const auto           pick = [&](const auto& opcodes) {
                return opcodes[gen() % opcodes.size()];
            };

            // The opcode is drawn before the mode: the order of the
            // arguments of a call is not specified, it would change the
            // programs with the compiler.
            const auto pick_with = [&](const auto& opcodes, const auto& in) {
                const auto op = pick(opcodes);
                return with_mode(op, pick(in));
            };

            constexpr std::array zero_page_or_absolute{ addressing::zero_page,
                                                        addressing::absolute };

            // The instructions without jump, branch nor stack. A write at
            // $zp,X or through ($zp,X) could go anywhere, over the code.
            const auto plain = [&]() {
                const auto kind = gen() % 24;
                return kind < 7    ? pick_with(immediate_opcodes, modes)
                       : kind < 9  ? pick(index_opcodes)
                       : kind < 10 ? pick(to_a_opcodes)
                       : kind < 12 ? pick_with(index_load_and_compare_opcodes,
                                               index_modes)
                       : kind < 13 ? pick_with(index_loads, indexed_modes)
                       : kind < 15
                         ? with_mode(opcode::sta_abs, pick(store_modes))
                       : kind < 16
                         ? pick_with(index_store_opcodes, zero_page_or_absolute)
                       : kind < 17 ? pick(shift_opcodes)
                       : kind < 19 ? pick(flag_opcodes)
                       : kind < 20
                         ? with_mode(opcode::bit_zp,
                                     pick(zero_page_or_absolute))
                         : pick_with(read_modify_write_opcodes, modify_modes);
            };

            // The subroutines follow a JMP $0080, without jump nor branch,
            // and leave the stack as they find it. The pointers of JMP
            // ($addr) follow them in 0x34..0x3d, then the pointer of STA
            // ($zp),Y in 0x3e, the zero page writes stay in 0x40..0x7f. The
            // main code lies around the page 0x0100, after a LDX #$C0, TXS
            // and the writes of the vector of BRK: some branches cross the
            // page and the stack, above the code, neither reaches it with the
            // pushes nor wraps with the pulls. The handler of BRK follows the
            // main code.
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
                for (int i = 0, e = 1 + static_cast<int>(gen() % 3); i < e; ++i)
                    add(plain());
                if (balance)
                    add(opcode::pla);
                add(opcode::rts);
            }
            constexpr int pointers_begin = 0x34;
            constexpr int jump_pointers  = 5;
            constexpr int store_pointer  = pointers_begin + 2 * jump_pointers;
            expect(fatal(le(size, pointers_begin)));

            constexpr int origin     = 0x8d;
            const auto    main_begin = ops.ssize();
            size                     = origin;
            while (size < origin + 256) {
                const auto kind = gen() % 24;
                add(kind < 15 ? plain()
                    : kind < 16
                      ? (gen() % 2 ? opcode::jmp_abs : opcode::jmp_ind)
                    : kind < 18 ? pick(branch_opcodes)
                    : kind < 19 ? (gen() % 2 ? opcode::pha : opcode::php)
                    : kind < 20 ? (gen() % 2 ? opcode::pla : opcode::plp)
                    : kind < 21 ? opcode::brk
                                : opcode::jsr_abs);
            }
            add(opcode::jmp_abs);
            const auto main_end = ops.ssize();

            const auto handler = size;
            for (int i = 0, e = 1 + static_cast<int>(gen() % 3); i < e; ++i)
                add(plain());
            add(opcode::rti);

            // The jumps and the branches go to the start of another
            // instruction of the main code, the jumps a little forward: the
            // loops come from the branches and the code runs to its end. The
            // writes stay in a window above the program, around a page: the
            // reads are in this window or in the program.
            const auto main_starts = std::span(starts.data() + main_begin,
                                               starts.data() + main_end);
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

            // A JMP ($addr) beyond the pointers becomes a JMP $addr.
            irt::vector<int> pointers(jump_pointers, irt::reserve_tag);

            put({ 0x4c, 0x80, 0x00 }); // JMP $0080
            for (int i = 0; i < ops.ssize(); ++i) {
                if (i == main_begin) {
                    while (program.ssize() < 0x80)
                        put({ 0x00 });
                    put({ 0xa2, 0xc0, 0x9a }); // LDX #$C0, TXS
                    put({ 0xa9, handler & 0xff, 0x8d, 0xfe, 0xff });
                    put({ 0xa9, handler >> 8, 0x8d, 0xff, 0xff });
                }

                auto op = ops[i];
                if (op == opcode::jmp_ind and pointers.ssize() == jump_pointers)
                    op = opcode::jmp_abs;
                put({ static_cast<int>(op) });

                const auto mode     = has_modes(op) ? mode_of(op)
                                                    : addressing::immediate;
                const auto absolute = has_modes(op) and
                                      has_absolute_address(mode);
                const auto store    = writes_at_address(op);

                if (i + 1 == main_end) {
                    put({ 0x00, 0x00 }); // JMP $0000
                } else if (op == opcode::jmp_ind) {
                    put(
                      { static_cast<int>(pointers_begin + 2 * pointers.ssize()),
                        0x00 });
                    pointers.emplace_back(target(starts[i], false));
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
                } else if (store and mode == addressing::indirect_y) {
                    put({ store_pointer });
                } else if (store) {
                    put({ static_cast<int>(0x40u + (gen() & 0x3fu)) });
                } else if (instruction_size(op) == 2) {
                    put({ static_cast<int>(gen() & 0xffu) });
                }
            }
            expect(fatal(eq(program.ssize(), size)));
            expect(fatal(le(size, 512)));

            for (int k = 0; k < pointers.ssize(); ++k) {
                program[pointers_begin + 2 * k] = static_cast<irt::u8>(
                  pointers[k] & 0xff);
                program[pointers_begin + 2 * k + 1] = static_cast<irt::u8>(
                  pointers[k] >> 8);
            }
            program[store_pointer]     = 0xc0;
            program[store_pointer + 1] = 0x10;

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
            total.read_modify_writes += run.read_modify_writes;
            total.indirect_jumps += run.indirect_jumps;
            total.pointer_operands += run.pointer_operands;
            total.pointer_crossings += run.pointer_crossings;
            total.decimal_operations += run.decimal_operations;
            total.breaks += run.breaks;
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
        expect(gt(total.read_modify_writes, 0));
        expect(gt(total.indirect_jumps, 0));
        expect(gt(total.pointer_operands, total.pointer_crossings));
        expect(gt(total.pointer_crossings, 0));
        expect(gt(total.decimal_operations, 0));
        expect(gt(total.breaks, 0));
        expect(gt(total.compares_greater, 0));
        expect(gt(total.compares_equal, 0));
        expect(gt(total.compares_lower, 0));
    };

    "cpu-survives-a-round-trip-through-irt-files"_test = [] {
        const auto  temp = irt::temp_path_with_unlink{ "cpu" };
        expect(fatal(temp.success()));
        const std::string_view dir = temp.c_str();

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
                    .comparator  = as_circuit(adder_8),
                    .alu         = alu
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

            expect(fatal(save_circuits(mod, dir,
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
                                         { compo[12], "cpu-bench.irt" } })
                           .has_value()));
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "cpu-bench.irt", 2, 55);
        bench_simulation sim(mod, b);
        check_cpu(sim, program, 30);
    };

    "generate-writes-the-system-components"_test = [] {
        const auto temp = irt::temp_path_with_unlink{ "generate" };
        expect(fatal(temp.success()));
        expect(fatal(generate(temp.c_str()).has_value()));

        const auto dir = std::filesystem::path(temp.c_str()) / "digital";
        for (const auto* file :
             { "full-adder.irt", "adder-8.irt", "adder-16.irt", "alu-8.irt",
               "alu-bench.irt", "register-8.irt", "accumulator.irt",
               "flags.irt", "index-registers.irt", "stack-pointer.irt",
               "program-counter.irt", "cpu-demo-memory.irt", "cpu-demo.irt",
               "cpu-demo-bench.irt" })
            expect(std::filesystem::exists(dir / file)) << file;

        // One real per byte, padded to a chunk of the binary file source.
        std::error_code ec;
        expect(eq(std::filesystem::file_size(dir / demo_program_file, ec),
                  irt::external_source_chunk_size * sizeof(double)));

        irt::modeling mod;
        const auto    b = load_bench(mod, temp.c_str(), "cpu-demo-bench.irt", 2,
                                     55);
        bench_simulation sim(mod, b);
        check_cpu(sim, demo_program(), 200);
    };

    "alu-8-bits-survives-a-round-trip-through-irt-files"_test = [] {
        const auto  temp = irt::temp_path_with_unlink{ "digital-circuits" };
        expect(fatal(temp.success()));
        const std::string_view dir = temp.c_str();

        {
            irt::modeling mod;
            const auto    compo = mod.ids.write([&](auto& ids) {
                const auto fa    = make_full_adder(ids);
                const auto adder = make_adder<8>(ids, fa);
                const auto alu   = make_alu(ids, adder);
                const auto b     = make_bench(ids, alu);
                return std::array{ fa.id, adder.id, alu.id, b.id };
            });

            expect(fatal(save_circuits(mod, dir,
                                       { { compo[0], "full-adder.irt" },
                                         { compo[1], "adder-8.irt" },
                                         { compo[2], "alu-8.irt" },
                                         { compo[3], "alu-bench.irt" } })
                           .has_value()));
        }

        irt::modeling    mod;
        const auto       b = load_bench(mod, dir, "alu-bench.irt", 21, 12);
        bench_simulation sim(mod, b);
        check_alu(sim);
    };
}
