#pragma once

#include <stdint.h>

#include "microkernel/arch/riscv/isa.h"

namespace jixia::arch::riscv {

enum class InstructionLength : uint8_t
{
    unsupported = 0,
    bytes_2 = 2,
    bytes_4 = 4,
};

enum class BreakpointInstruction : uint8_t
{
    none = 0,
    compressed,
    standard,
};

struct BreakpointDecode
{
    BreakpointInstruction instruction;
    InstructionLength length;

    [[nodiscard]]
    constexpr bool recognized() const
    {
        return instruction != BreakpointInstruction::none;
    }
};

inline constexpr uint16_t compressed_ebreak_encoding = 0x9002U;
inline constexpr uint32_t standard_ebreak_encoding = 0x00100073U;

[[nodiscard]]
constexpr InstructionLength decode_instruction_length(
    uint16_t first_parcel)
{
    /* Low bits other than 11 identify a 16-bit compressed instruction. */
    if ((first_parcel & 0x3U) != 0x3U)
    {
        return InstructionLength::bytes_2;
    }

    /* 32-bit instructions have low bits 11 but not low five bits 11111. */
    if ((first_parcel & 0x1FU) != 0x1FU)
    {
        return InstructionLength::bytes_4;
    }

    /* M00-03 deliberately rejects 48-bit and longer encodings. */
    return InstructionLength::unsupported;
}

[[nodiscard]]
constexpr Xlen instruction_length_bytes(InstructionLength length)
{
    return static_cast<Xlen>(length);
}

/*
 * Decode only EBREAK and C.EBREAK at a trusted executable address.
 * The implementation performs byte loads, so a 32-bit instruction starting
 * at a two-byte-aligned address is handled without an unaligned uint32_t load.
 */
[[nodiscard]]
BreakpointDecode decode_breakpoint_at(Xlen pc);

static_assert(
    decode_instruction_length(compressed_ebreak_encoding) ==
    InstructionLength::bytes_2);
static_assert(
    decode_instruction_length(
        static_cast<uint16_t>(standard_ebreak_encoding & 0xFFFFU)) ==
    InstructionLength::bytes_4);
static_assert(
    decode_instruction_length(0x001FU) ==
    InstructionLength::unsupported);

} // namespace jixia::arch::riscv
