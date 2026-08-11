#pragma once

/*
 * RV64 machine-mode trap-frame ABI.
 *
 * This header is shared by C++ and preprocessed assembly (.S).
 * Keep every offset numeric and identical on both sides of the ABI.
 */

#define TRAP_FRAME_X0_OFFSET        0
#define TRAP_FRAME_X1_OFFSET        8
#define TRAP_FRAME_X2_OFFSET        16
#define TRAP_FRAME_X3_OFFSET        24
#define TRAP_FRAME_X4_OFFSET        32
#define TRAP_FRAME_X5_OFFSET        40
#define TRAP_FRAME_X6_OFFSET        48
#define TRAP_FRAME_X7_OFFSET        56
#define TRAP_FRAME_X8_OFFSET        64
#define TRAP_FRAME_X9_OFFSET        72
#define TRAP_FRAME_X10_OFFSET       80
#define TRAP_FRAME_X11_OFFSET       88
#define TRAP_FRAME_X12_OFFSET       96
#define TRAP_FRAME_X13_OFFSET       104
#define TRAP_FRAME_X14_OFFSET       112
#define TRAP_FRAME_X15_OFFSET       120
#define TRAP_FRAME_X16_OFFSET       128
#define TRAP_FRAME_X17_OFFSET       136
#define TRAP_FRAME_X18_OFFSET       144
#define TRAP_FRAME_X19_OFFSET       152
#define TRAP_FRAME_X20_OFFSET       160
#define TRAP_FRAME_X21_OFFSET       168
#define TRAP_FRAME_X22_OFFSET       176
#define TRAP_FRAME_X23_OFFSET       184
#define TRAP_FRAME_X24_OFFSET       192
#define TRAP_FRAME_X25_OFFSET       200
#define TRAP_FRAME_X26_OFFSET       208
#define TRAP_FRAME_X27_OFFSET       216
#define TRAP_FRAME_X28_OFFSET       224
#define TRAP_FRAME_X29_OFFSET       232
#define TRAP_FRAME_X30_OFFSET       240
#define TRAP_FRAME_X31_OFFSET       248

#define TRAP_FRAME_MSTATUS_OFFSET   256
#define TRAP_FRAME_MEPC_OFFSET      264
#define TRAP_FRAME_MCAUSE_OFFSET    272
#define TRAP_FRAME_MTVAL_OFFSET     280

#define TRAP_FRAME_SIZE             288
#define TRAP_FRAME_ALIGNMENT        16

#ifndef __ASSEMBLER__

#include <stddef.h>

#include "microkernel/arch/riscv/isa.h"

namespace jixia::arch::riscv {

struct alignas(TRAP_FRAME_ALIGNMENT) TrapFrame {
    /*
     * General-purpose registers indexed by architectural register number.
     * x[0] is explicitly written as zero by the entry path.
     */
    Xlen x[32];

    Xlen mstatus;
    Xlen mepc;
    Xlen mcause;
    Xlen mtval;
};

static_assert(sizeof(Xlen) == 8U);
static_assert(alignof(TrapFrame) == TRAP_FRAME_ALIGNMENT);
static_assert(sizeof(TrapFrame) == TRAP_FRAME_SIZE);
static_assert((TRAP_FRAME_SIZE % TRAP_FRAME_ALIGNMENT) == 0);

#define ASSERT_TRAP_FRAME_X_OFFSET(index)                    \
    static_assert(                                           \
        offsetof(TrapFrame, x) + (sizeof(Xlen) * (index)) == \
        TRAP_FRAME_X##index##_OFFSET)

ASSERT_TRAP_FRAME_X_OFFSET(0);
ASSERT_TRAP_FRAME_X_OFFSET(1);
ASSERT_TRAP_FRAME_X_OFFSET(2);
ASSERT_TRAP_FRAME_X_OFFSET(3);
ASSERT_TRAP_FRAME_X_OFFSET(4);
ASSERT_TRAP_FRAME_X_OFFSET(5);
ASSERT_TRAP_FRAME_X_OFFSET(6);
ASSERT_TRAP_FRAME_X_OFFSET(7);
ASSERT_TRAP_FRAME_X_OFFSET(8);
ASSERT_TRAP_FRAME_X_OFFSET(9);
ASSERT_TRAP_FRAME_X_OFFSET(10);
ASSERT_TRAP_FRAME_X_OFFSET(11);
ASSERT_TRAP_FRAME_X_OFFSET(12);
ASSERT_TRAP_FRAME_X_OFFSET(13);
ASSERT_TRAP_FRAME_X_OFFSET(14);
ASSERT_TRAP_FRAME_X_OFFSET(15);
ASSERT_TRAP_FRAME_X_OFFSET(16);
ASSERT_TRAP_FRAME_X_OFFSET(17);
ASSERT_TRAP_FRAME_X_OFFSET(18);
ASSERT_TRAP_FRAME_X_OFFSET(19);
ASSERT_TRAP_FRAME_X_OFFSET(20);
ASSERT_TRAP_FRAME_X_OFFSET(21);
ASSERT_TRAP_FRAME_X_OFFSET(22);
ASSERT_TRAP_FRAME_X_OFFSET(23);
ASSERT_TRAP_FRAME_X_OFFSET(24);
ASSERT_TRAP_FRAME_X_OFFSET(25);
ASSERT_TRAP_FRAME_X_OFFSET(26);
ASSERT_TRAP_FRAME_X_OFFSET(27);
ASSERT_TRAP_FRAME_X_OFFSET(28);
ASSERT_TRAP_FRAME_X_OFFSET(29);
ASSERT_TRAP_FRAME_X_OFFSET(30);
ASSERT_TRAP_FRAME_X_OFFSET(31);

#undef ASSERT_TRAP_FRAME_X_OFFSET

static_assert(offsetof(TrapFrame, mstatus) == TRAP_FRAME_MSTATUS_OFFSET);
static_assert(offsetof(TrapFrame, mepc) == TRAP_FRAME_MEPC_OFFSET);
static_assert(offsetof(TrapFrame, mcause) == TRAP_FRAME_MCAUSE_OFFSET);
static_assert(offsetof(TrapFrame, mtval) == TRAP_FRAME_MTVAL_OFFSET);

} // namespace jixia::arch::riscv

#endif
