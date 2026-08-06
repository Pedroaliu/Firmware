#pragma once

/*
 * Jixia RV64 machine-mode trap-frame ABI.
 *
 * This header is shared by C++ and preprocessed assembly (.S).
 * Keep every offset numeric and identical on both sides of the ABI.
 */

#define JIXIA_TRAP_X0_OFFSET        0
#define JIXIA_TRAP_X1_OFFSET        8
#define JIXIA_TRAP_X2_OFFSET        16
#define JIXIA_TRAP_X3_OFFSET        24
#define JIXIA_TRAP_X4_OFFSET        32
#define JIXIA_TRAP_X5_OFFSET        40
#define JIXIA_TRAP_X6_OFFSET        48
#define JIXIA_TRAP_X7_OFFSET        56
#define JIXIA_TRAP_X8_OFFSET        64
#define JIXIA_TRAP_X9_OFFSET        72
#define JIXIA_TRAP_X10_OFFSET       80
#define JIXIA_TRAP_X11_OFFSET       88
#define JIXIA_TRAP_X12_OFFSET       96
#define JIXIA_TRAP_X13_OFFSET       104
#define JIXIA_TRAP_X14_OFFSET       112
#define JIXIA_TRAP_X15_OFFSET       120
#define JIXIA_TRAP_X16_OFFSET       128
#define JIXIA_TRAP_X17_OFFSET       136
#define JIXIA_TRAP_X18_OFFSET       144
#define JIXIA_TRAP_X19_OFFSET       152
#define JIXIA_TRAP_X20_OFFSET       160
#define JIXIA_TRAP_X21_OFFSET       168
#define JIXIA_TRAP_X22_OFFSET       176
#define JIXIA_TRAP_X23_OFFSET       184
#define JIXIA_TRAP_X24_OFFSET       192
#define JIXIA_TRAP_X25_OFFSET       200
#define JIXIA_TRAP_X26_OFFSET       208
#define JIXIA_TRAP_X27_OFFSET       216
#define JIXIA_TRAP_X28_OFFSET       224
#define JIXIA_TRAP_X29_OFFSET       232
#define JIXIA_TRAP_X30_OFFSET       240
#define JIXIA_TRAP_X31_OFFSET       248

#define JIXIA_TRAP_MSTATUS_OFFSET   256
#define JIXIA_TRAP_MEPC_OFFSET      264
#define JIXIA_TRAP_MCAUSE_OFFSET    272
#define JIXIA_TRAP_MTVAL_OFFSET     280

#define JIXIA_TRAP_FRAME_SIZE       288
#define JIXIA_TRAP_FRAME_ALIGNMENT  16

#ifndef __ASSEMBLER__

#include <stddef.h>
#include <stdint.h>

namespace jixia::arch::riscv {

struct alignas(JIXIA_TRAP_FRAME_ALIGNMENT) TrapFrame {
    /*
     * General-purpose registers indexed by architectural register number.
     * x[0] is explicitly written as zero by the entry path.
     */
    uintptr_t x[32];

    uintptr_t mstatus;
    uintptr_t mepc;
    uintptr_t mcause;
    uintptr_t mtval;
};

static_assert(sizeof(uintptr_t) == 8);
static_assert(alignof(TrapFrame) == JIXIA_TRAP_FRAME_ALIGNMENT);
static_assert(sizeof(TrapFrame) == JIXIA_TRAP_FRAME_SIZE);
static_assert(
    (JIXIA_TRAP_FRAME_SIZE % JIXIA_TRAP_FRAME_ALIGNMENT) == 0);

#define JIXIA_ASSERT_TRAP_X_OFFSET(index)                         \
    static_assert(                                                \
        offsetof(TrapFrame, x) + (sizeof(uintptr_t) * (index)) == \
        JIXIA_TRAP_X##index##_OFFSET)

JIXIA_ASSERT_TRAP_X_OFFSET(0);
JIXIA_ASSERT_TRAP_X_OFFSET(1);
JIXIA_ASSERT_TRAP_X_OFFSET(2);
JIXIA_ASSERT_TRAP_X_OFFSET(3);
JIXIA_ASSERT_TRAP_X_OFFSET(4);
JIXIA_ASSERT_TRAP_X_OFFSET(5);
JIXIA_ASSERT_TRAP_X_OFFSET(6);
JIXIA_ASSERT_TRAP_X_OFFSET(7);
JIXIA_ASSERT_TRAP_X_OFFSET(8);
JIXIA_ASSERT_TRAP_X_OFFSET(9);
JIXIA_ASSERT_TRAP_X_OFFSET(10);
JIXIA_ASSERT_TRAP_X_OFFSET(11);
JIXIA_ASSERT_TRAP_X_OFFSET(12);
JIXIA_ASSERT_TRAP_X_OFFSET(13);
JIXIA_ASSERT_TRAP_X_OFFSET(14);
JIXIA_ASSERT_TRAP_X_OFFSET(15);
JIXIA_ASSERT_TRAP_X_OFFSET(16);
JIXIA_ASSERT_TRAP_X_OFFSET(17);
JIXIA_ASSERT_TRAP_X_OFFSET(18);
JIXIA_ASSERT_TRAP_X_OFFSET(19);
JIXIA_ASSERT_TRAP_X_OFFSET(20);
JIXIA_ASSERT_TRAP_X_OFFSET(21);
JIXIA_ASSERT_TRAP_X_OFFSET(22);
JIXIA_ASSERT_TRAP_X_OFFSET(23);
JIXIA_ASSERT_TRAP_X_OFFSET(24);
JIXIA_ASSERT_TRAP_X_OFFSET(25);
JIXIA_ASSERT_TRAP_X_OFFSET(26);
JIXIA_ASSERT_TRAP_X_OFFSET(27);
JIXIA_ASSERT_TRAP_X_OFFSET(28);
JIXIA_ASSERT_TRAP_X_OFFSET(29);
JIXIA_ASSERT_TRAP_X_OFFSET(30);
JIXIA_ASSERT_TRAP_X_OFFSET(31);

#undef JIXIA_ASSERT_TRAP_X_OFFSET

static_assert(offsetof(TrapFrame, mstatus) == JIXIA_TRAP_MSTATUS_OFFSET);
static_assert(offsetof(TrapFrame, mepc) == JIXIA_TRAP_MEPC_OFFSET);
static_assert(offsetof(TrapFrame, mcause) == JIXIA_TRAP_MCAUSE_OFFSET);
static_assert(offsetof(TrapFrame, mtval) == JIXIA_TRAP_MTVAL_OFFSET);

} // namespace jixia::arch::riscv

#endif
