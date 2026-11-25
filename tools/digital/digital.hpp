// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef ORG_VLEPROJECT_IRRITATOR_TOOLS_DIGITAL_2026
#define ORG_VLEPROJECT_IRRITATOR_TOOLS_DIGITAL_2026

#include <irritator/core.hpp>
#include <irritator/modeling.hpp>

#include <array>
#include <filesystem>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

/** Digital circuits built from the logical models and nested generic
 * components, up to a 6502. The builders abort on an allocation failure. */
namespace irt::digital {

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

struct full_adder_component {
    irt::component_id id;
    irt::port_id      a, b, cin, s, cout;
};

/** s = a xor b xor cin and cout = (a and b) or ((a xor b) and cin). */
full_adder_component make_full_adder(irt::component_access& ids) noexcept;

template<int Bits>
struct adder_component {
    irt::component_id              id;
    std::array<irt::port_id, Bits> a, b, s;
    irt::port_id                   cin, cout;
};

/** A ripple-carry adder: the carry of the bit i feeds the bit i + 1. */
template<int Bits>
adder_component<Bits> make_adder(irt::component_access&      ids,
                                 const full_adder_component& fa) noexcept;

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

/** An 8 bits ALU: a decoder of the operation selects, for each bit of the
 * result, one of the candidates computed in parallel. With d, ADC and SBC
 * correct the digits of the binary sum. */
circuit make_alu(irt::component_access&    ids,
                 const adder_component<8>& adder) noexcept;

/** A counter of @c Bits bits: a register of D flip-flops fed by the
 * incrementer of its own outputs, one increment per rising edge. */
template<int Bits>
circuit make_counter(irt::component_access& ids) noexcept;

/** A register of @c Bits bits: on a rising edge of clk, q takes d if load is
 * true and keeps its value otherwise. Inputs: d0.. then load. Outputs: q0..
 */
template<int Bits>
circuit make_register(irt::component_access& ids) noexcept;

/** The accumulator of the 6502: the ALU computes from A and the inputs, and
 * its result is loaded in A on a rising edge when load is true. Inputs: b0..b7,
 * cin, op0..op2, load then d, the decimal mode. Outputs: a0..a7 then the c,
 * v, n and z flags of the ALU. */
circuit make_accumulator(irt::component_access& ids,
                         const circuit&         alu,
                         const circuit&         reg) noexcept;

/** The C, V, N, Z, I and D flags of the status register P: C, V, I and D
 * are loaded apart, N and Z together, as the 6502 instructions do. Inputs:
 * c, v, n, z, load_c, load_v, load_nz, i, d, load_i then load_d. Outputs: c,
 * v, n, z, i then d. */
circuit make_flags(irt::component_access& ids) noexcept;

/** The execution unit: the accumulator and the flags, the carry of the ALU
 * is the stored C flag as for ADC, SBC, ROL and ROR. Inputs: b0..b7,
 * op0..op2, load_a, load_c, load_v then load_nz. Outputs: a0..a7 then the
 * stored c, v, n and z flags. */
circuit make_execution_unit(irt::component_access& ids,
                            const circuit&         acc,
                            const circuit&         flags) noexcept;

/** The program counter: on a rising edge, pc takes d if load is true (a jump),
 * pc + 1 if inc is true (the next byte), and keeps its value otherwise.
 * Inputs: d0..d15, load then inc. Outputs: pc0..pc15. */
circuit make_program_counter(irt::component_access& ids) noexcept;

/** A component around a logical_memory of 64 KiB, the first bytes are the @a
 * content read at the initialization from a constant source of the component,
 * or from the binary @a file which holds them. Inputs: a0..a15, d0..d7 then
 * we. Outputs: q0..q7. */
circuit make_memory(
  irt::component_access&   ids,
  std::span<const irt::u8> content = {},
  const irt::file_path_id  file = irt::undefined<irt::file_path_id>()) noexcept;

/** The index registers X and Y: on a rising edge, when write is true, the
 * register selected by sel_y takes d if imm is true, its value plus one
 * otherwise, or minus one if dec is true. Inputs: d0..d7, imm, dec, sel_y then
 * write. Outputs: x0..x7, y0..y7 then the n and z of the result. */
circuit make_index_registers(irt::component_access& ids,
                             const circuit&         reg) noexcept;

/** The stack pointer S: on a rising edge, S takes d if load is true, S + 1 if
 * inc is true or S - 1 if dec is true. Inputs: d0..d7, load, inc then dec.
 * Outputs: s0..s7. */
circuit make_stack_pointer(irt::component_access& ids,
                           const circuit&         reg) noexcept;

/** The opcodes of the 6502. The operations of the ALU, CMP, STA, LDX, LDY,
 * CPX, CPY, STX, STY, BIT and the read-modify-write operations exist with
 * other addressing modes, see with_mode(). */
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
    tax     = 0xaa,
    txa     = 0x8a,
    tay     = 0xa8,
    tya     = 0x98,
    stx_zp  = 0x86,
    sty_zp  = 0x84,
    asl_zp  = 0x06,
    rol_zp  = 0x26,
    lsr_zp  = 0x46,
    ror_zp  = 0x66,
    dec_zp  = 0xc6,
    inc_zp  = 0xe6,
    bpl     = 0x10,
    bmi     = 0x30,
    bvc     = 0x50,
    bvs     = 0x70,
    bcc     = 0x90,
    bcs     = 0xb0,
    clc     = 0x18,
    sec     = 0x38,
    clv     = 0xb8,
    cli     = 0x58,
    sei     = 0x78,
    cld     = 0xd8,
    sed     = 0xf8,
    bit_zp  = 0x24,
    jmp_ind = 0x6c,
    php     = 0x08,
    plp     = 0x28,
    brk     = 0x00,
    rti     = 0x40,
    nop     = 0xea,
};

/** The addressing modes: the bits 4..2 of the opcodes of the ALU, CMP and STA,
 * whose bits 1..0 are 01, and of the other opcodes with modes. LDX, LDY, CPX
 * and CPY use 000 for an immediate value, LDX and STX add Y instead of X. */
enum class addressing : irt::u8 {
    indirect_x  = 0b000,
    zero_page   = 0b001,
    immediate   = 0b010,
    absolute    = 0b011,
    indirect_y  = 0b100,
    zero_page_x = 0b101,
    absolute_y  = 0b110,
    absolute_x  = 0b111,
};

constexpr bool is_alu_or_sta(const opcode op) noexcept
{
    return (static_cast<unsigned>(op) & 0x03u) == 0x01u and
           static_cast<unsigned>(op) != 0x89u;
}

/** LDX, LDY, CPX and CPY in any addressing mode, LDX and LDY with an index
 * too. */
constexpr bool is_index_load_or_compare(const opcode op) noexcept
{
    const auto code = static_cast<unsigned>(op);
    const auto mode = (code >> 2) & 0x07u;
    const auto base = code & 0xe3u;
    const auto load = base == 0xa2u or base == 0xa0u;

    return (load or base == 0xe0u or base == 0xc0u) and
           (mode == 0b000u or mode == 0b001u or mode == 0b011u or
            (load and (mode == 0b101u or mode == 0b111u)));
}

/** STX and STY in the zero page, with an index too, and at an absolute
 * address. */
constexpr bool is_index_store(const opcode op) noexcept
{
    const auto code = static_cast<unsigned>(op);
    const auto mode = (code >> 2) & 0x07u;
    const auto base = code & 0xe3u;

    return (base == 0x82u or base == 0x80u) and
           (mode == 0b001u or mode == 0b011u or mode == 0b101u);
}

/** ASL, ROL, LSR, ROR, DEC and INC on a byte of the memory: the bits 1..0
 * are 10 and the bit 2 is set, the bits 7..5 are neither 100 (STX) nor 101
 * (LDX). */
constexpr bool is_read_modify_write(const opcode op) noexcept
{
    const auto code      = static_cast<unsigned>(op);
    const auto operation = code >> 5;

    return (code & 0x07u) == 0x06u and operation != 0b100u and
           operation != 0b101u;
}

/** BIT in the zero page and at an absolute address. */
constexpr bool is_bit(const opcode op) noexcept
{
    const auto code = static_cast<unsigned>(op);
    return code == 0x24u or code == 0x2cu;
}

constexpr bool has_modes(const opcode op) noexcept
{
    return is_alu_or_sta(op) or is_index_load_or_compare(op) or
           is_index_store(op) or is_read_modify_write(op) or is_bit(op);
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

/** LDX and STX add Y in the modes of X. */
constexpr bool indexes_with_y(const opcode op) noexcept
{
    const auto base = static_cast<unsigned>(op) & 0xe3u;
    const auto mode = mode_of(op);

    return mode == addressing::absolute_y or mode == addressing::indirect_y or
           ((base == 0xa2u or base == 0x82u) and
            (mode == addressing::zero_page_x or
             mode == addressing::absolute_x));
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
static_assert(with_mode(opcode::stx_zp, addressing::absolute) ==
              static_cast<opcode>(0x8e));
static_assert(with_mode(opcode::inc_zp, addressing::absolute_x) ==
              static_cast<opcode>(0xfe));
static_assert(not has_modes(opcode::txa) and not has_modes(opcode::tya) and
              not has_modes(opcode::tax) and not has_modes(opcode::asl_a) and
              not has_modes(opcode::txs) and not has_modes(opcode::dey) and
              not is_read_modify_write(opcode::stx_zp) and
              not is_read_modify_write(with_mode(opcode::ldx_imm,
                                                 addressing::zero_page)));
static_assert(with_mode(opcode::lda_imm, addressing::indirect_x) ==
              static_cast<opcode>(0xa1));
static_assert(with_mode(opcode::sta_abs, addressing::indirect_y) ==
              static_cast<opcode>(0x91));
static_assert(with_mode(opcode::ldx_imm, addressing::absolute_x) ==
              static_cast<opcode>(0xbe));
static_assert(with_mode(opcode::sty_zp, addressing::zero_page_x) ==
              static_cast<opcode>(0x94));
static_assert(indexes_with_y(static_cast<opcode>(0x96)) and
              indexes_with_y(static_cast<opcode>(0xb6)) and
              not indexes_with_y(static_cast<opcode>(0xbc)) and
              not has_modes(opcode::brk) and not has_modes(opcode::php) and
              not has_modes(opcode::nop) and
              not has_modes(static_cast<opcode>(0x9e)) and
              not has_modes(static_cast<opcode>(0xd4)) and
              not has_modes(static_cast<opcode>(0x89)));

constexpr bool is_store(const opcode op) noexcept
{
    return (is_alu_or_sta(op) and (static_cast<unsigned>(op) >> 5) == 0b100u) or
           is_index_store(op);
}

/** The instructions which write in the memory at their address. */
constexpr bool writes_at_address(const opcode op) noexcept
{
    return is_store(op) or is_read_modify_write(op);
}

constexpr bool is_compare(const opcode op) noexcept
{
    const auto base = static_cast<unsigned>(op) & 0xe3u;
    return (is_alu_or_sta(op) and base == 0xc1u) or
           (is_index_load_or_compare(op) and (base == 0xe0u or base == 0xc0u));
}

constexpr bool is_decimal_operation(const opcode op) noexcept
{
    const auto base = static_cast<unsigned>(op) & 0xe3u;
    return is_alu_or_sta(op) and (base == 0x61u or base == 0xe1u);
}

/** The operations of the ALU and CMP, the bits 7..5 of the opcodes select
 * them. */
constexpr std::array immediate_opcodes{ opcode::ora_imm, opcode::and_imm,
                                        opcode::eor_imm, opcode::adc_imm,
                                        opcode::lda_imm, opcode::cmp_imm,
                                        opcode::sbc_imm };

constexpr std::array index_opcodes{ opcode::inx, opcode::iny, opcode::dex,
                                    opcode::dey, opcode::tsx, opcode::tax,
                                    opcode::tay };

constexpr std::array to_a_opcodes{ opcode::txa, opcode::tya };

constexpr std::array index_store_opcodes{ opcode::stx_zp, opcode::sty_zp };

constexpr std::array read_modify_write_opcodes{
    opcode::asl_zp, opcode::rol_zp, opcode::lsr_zp,
    opcode::ror_zp, opcode::dec_zp, opcode::inc_zp
};

constexpr std::array index_load_and_compare_opcodes{
    opcode::ldx_imm, opcode::ldy_imm, opcode::cpx_imm, opcode::cpy_imm
};

constexpr std::array shift_opcodes{ opcode::asl_a, opcode::rol_a, opcode::lsr_a,
                                    opcode::ror_a };

/** The branches: the bits 7..6 select the flag N, V, C or Z, the branch is
 * taken if it equals the bit 5. */
constexpr bool is_branch(const opcode op) noexcept
{
    return (static_cast<unsigned>(op) & 0x1fu) == 0x10u;
}

constexpr std::array branch_opcodes{ opcode::bpl, opcode::bmi, opcode::bvc,
                                     opcode::bvs, opcode::bcc, opcode::bcs,
                                     opcode::bne, opcode::beq };

constexpr std::array flag_opcodes{ opcode::clc, opcode::sec, opcode::clv,
                                   opcode::cli, opcode::sei, opcode::cld,
                                   opcode::sed, opcode::nop };

constexpr bool has_absolute_address(const addressing mode) noexcept
{
    return mode == addressing::absolute or mode == addressing::absolute_x or
           mode == addressing::absolute_y;
}

/** The size in bytes of an instruction. BRK skips the byte which follows
 * it. */
constexpr int instruction_size(const opcode op) noexcept
{
    if (has_modes(op))
        return has_absolute_address(mode_of(op)) ? 3 : 2;

    switch (op) {
    case opcode::jmp_abs:
    case opcode::jmp_ind:
    case opcode::jsr_abs:
        return 3;
    case opcode::brk:
        return 2;
    default:
        return is_branch(op) ? 2 : 1;
    }
}

/** The components of the CPU: the 8 bits adder compares a register to the
 * operand, the ALU also modifies the bytes of the read-modify-write
 * operations. */
struct cpu_parts {
    circuit pc, memory, accumulator, flags, index, stack, reg, adder,
      comparator, alu;
};

/** A 6502 with the cycles of the 6502: one memory access per cycle. The
 * program starts at address 0, the stack is the page 0x0100 under S. A
 * one-hot sequencer of flip-flops t1 to t6 follows the fetch cycle t0, when
 * none of them is true:
 * - t0: ir takes the opcode, pc + 1;
 * - t1: the immediate or implied operation on A, X, Y, S or P executes, or a
 *   comparison, or adl takes the low byte of the address, the zero page
 *   address of a pointer or the offset of a branch; pc + 1 except for the
 *   implied operations;
 * - t2: a jump or a taken branch loads pc, or adh takes the high byte of the
 *   address and pc + 1, or the operation reads its operand at the zero page
 *   address or a store writes, or PHA and PHP push, or the pulls increment
 *   S, or BRK pushes the high byte of pc;
 * - t3: the operation reads its operand at the address or a store writes, or
 *   the address plus an index crosses a page, or the taken branch crosses a
 *   page, or PLA and PLP pull, or JSR pushes the high byte of pc, or RTS
 *   pulls adl, RTI pulls P, BRK pushes the low byte of pc;
 * - t4: the operation reads its operand at the address plus an index or a
 *   store writes, or JSR pushes the low byte of pc, or RTS pulls the high
 *   byte of pc, RTI pulls adl, BRK pushes P;
 * - t5: JSR reads the high byte of the address and jumps, or RTS adds one to
 *   pc, RTI pulls the high byte of pc, BRK reads the low byte of its vector
 *   and sets I;
 * - t6: BRK reads the high byte of its vector and jumps;
 * - t2 to t6: a read-modify-write operation reads the byte at the end of its
 *   address, with the extra cycle of an index, modifies it in the next cycle
 *   and writes it in the one after;
 * - t2 to t5: ($zp,X) reads the pointer at zp + X in t3 and t4, ($zp),Y at
 *   zp in t2 and t3, the operand follows as for an absolute address plus an
 *   index.
 *
 * The 16 bits adder computes the target of a branch, pc plus the offset, or
 * the address plus X or Y. The 8 bits adder compares A, X or Y to the operand.
 *
 * The interrupts are polled on the rising edge which ends t0, see
 * interrupt_lines: a request replaces the opcode by BRK, which then keeps pc,
 * pushes P without B and reads the vector of NMI or of IRQ. Inputs: irq then
 * nmi. Outputs: pc0..pc15, a0..a7, x0..x7, y0..y7, s0..s7, c, v, n, z, i, d
 * then sync, true in t0. */
circuit make_cpu(irt::component_access& ids, const cpu_parts& parts) noexcept;

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

bench make_bench(irt::component_access& ids, const circuit& tested) noexcept;

/** The directory @c dir/digital of the circuits, created if needed. */
irt::dir_path_id make_circuits_dir(irt::modeling&         mod,
                                   const std::string_view dir) noexcept;

/** Writes the bytes of a program as the reals of a binary file source: one
 * real per byte, padded with zeros to a chunk of the source. */
irt::status write_program_file(const std::filesystem::path&   file,
                               const std::span<const irt::u8> bytes) noexcept;

/** Saves the components in the directory @c dir_id, in the order of their
 * dependencies: a file references the files of its children. */
irt::status save_circuits(
  irt::modeling&                                                   mod,
  const irt::dir_path_id                                           dir_id,
  std::initializer_list<std::pair<irt::component_id, const char*>> files);

/** Saves the components in the directory @c dir/digital. */
irt::status save_circuits(
  irt::modeling&                                                   mod,
  const std::string_view                                           dir,
  std::initializer_list<std::pair<irt::component_id, const char*>> files);

/** The parts of a CPU whose memory holds @a program from the address 0. */
cpu_parts make_cpu_parts(irt::component_access&   ids,
                         std::span<const irt::u8> program) noexcept;

/** A bench around a CPU whose memory holds @a program from the address 0. */
bench make_cpu_bench(irt::component_access&   ids,
                     std::span<const irt::u8> program) noexcept;

/** The name of the file of the demonstration program, next to its memory. */
constexpr std::string_view demo_program_file = "6502-demo.bin";

/** The demonstration program, from the address 0: the Fibonacci numbers in
 * decimal, stored from $0300 until they pass 99, then again. X counts the
 * numbers. */
std::span<const irt::u8> demo_program() noexcept;

/** Writes the components of the 6502 in @c dir/digital, to open them as the
 * system components of the graphical interface: the parts of the CPU, a
 * bench of the ALU, and a bench of a CPU whose memory reads the demonstration
 * program from @c demo_program_file. */
irt::status generate(std::string_view dir) noexcept;

} // namespace irt::digital

#endif
