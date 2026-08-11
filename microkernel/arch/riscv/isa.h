#pragma once

#include <limits.h>
#include <stdint.h>

#if !defined(__riscv)
#error "Jixia RISC-V architecture code requires a RISC-V compiler"
#endif

#if !defined(__riscv_xlen)
#error "The RISC-V compiler must define __riscv_xlen"
#endif

namespace jixia::arch::riscv {

/*
 * The firmware binary is compiled for one XLEN. Runtime code cannot switch
 * between RV32 and RV64: the instruction set and ABI have already been fixed
 * by the compiler options.
 */
using Xlen = uintptr_t;

inline constexpr unsigned xlen_bits = __riscv_xlen;

static_assert(xlen_bits == 64U);
static_assert((sizeof(Xlen) * CHAR_BIT) == xlen_bits);

inline constexpr Xlen xlen_most_significant_bit =
    Xlen{1} << (xlen_bits - 1U);

inline constexpr Xlen mcause_interrupt_mask =
    xlen_most_significant_bit;

inline constexpr Xlen mcause_code_mask =
    mcause_interrupt_mask - Xlen{1};

/* misa.MXL uses the same architectural encoding in the top two bits. */
enum class MachineXlenEncoding : Xlen
{
    unknown = 0,
    rv32 = 1,
    rv64 = 2,
    rv128 = 3,
};

[[nodiscard]]
inline Xlen read_misa()
{
    Xlen value;
    __asm__ volatile("csrr %0, misa" : "=r"(value));
    return value;
}

[[nodiscard]]
constexpr MachineXlenEncoding decode_misa_mxl(Xlen misa)
{
    constexpr Xlen mxl_mask = 0x3U;
    const Xlen encoded = (misa >> (xlen_bits - 2U)) & mxl_mask;
    return static_cast<MachineXlenEncoding>(encoded);
}

} // namespace jixia::arch::riscv
