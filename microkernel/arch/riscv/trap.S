#include "microkernel/arch/riscv/trap_frame.h"

.section .text.trap, "ax", @progbits

.balign 4
.global jixia_trap_entry
.type jixia_trap_entry, @function

.macro SAVE_GPR reg, offset
    sd \reg, \offset(sp)
.endm

.macro RESTORE_GPR reg, offset
    ld \reg, \offset(sp)
.endm

.macro SAVE_CSR csr, offset
    csrr t0, \csr
    sd   t0, \offset(sp)
.endm

.macro RESTORE_CSR csr, offset
    ld   t0, \offset(sp)
    csrw \csr, t0
.endm

/*
 * Save a complete RV64 integer context on the interrupted stack.
 *
 * This milestone intentionally has no dedicated per-hart trap stack, stack
 * guard, or nesting policy yet. Interrupts remain disabled while this frame
 * is live.
 */
jixia_trap_entry:
    addi sp, sp, -TRAP_FRAME_SIZE

    /* t0 must be preserved before it is used as the entry scratch register. */
    SAVE_GPR t0,   TRAP_FRAME_X5_OFFSET

    SAVE_GPR zero, TRAP_FRAME_X0_OFFSET
    SAVE_GPR ra,   TRAP_FRAME_X1_OFFSET

    /* Recover and record the interrupted value of sp. */
    addi t0, sp, TRAP_FRAME_SIZE
    sd   t0, TRAP_FRAME_X2_OFFSET(sp)

    SAVE_GPR gp,   TRAP_FRAME_X3_OFFSET
    SAVE_GPR tp,   TRAP_FRAME_X4_OFFSET
    SAVE_GPR t1,   TRAP_FRAME_X6_OFFSET
    SAVE_GPR t2,   TRAP_FRAME_X7_OFFSET
    SAVE_GPR s0,   TRAP_FRAME_X8_OFFSET
    SAVE_GPR s1,   TRAP_FRAME_X9_OFFSET
    SAVE_GPR a0,   TRAP_FRAME_X10_OFFSET
    SAVE_GPR a1,   TRAP_FRAME_X11_OFFSET
    SAVE_GPR a2,   TRAP_FRAME_X12_OFFSET
    SAVE_GPR a3,   TRAP_FRAME_X13_OFFSET
    SAVE_GPR a4,   TRAP_FRAME_X14_OFFSET
    SAVE_GPR a5,   TRAP_FRAME_X15_OFFSET
    SAVE_GPR a6,   TRAP_FRAME_X16_OFFSET
    SAVE_GPR a7,   TRAP_FRAME_X17_OFFSET
    SAVE_GPR s2,   TRAP_FRAME_X18_OFFSET
    SAVE_GPR s3,   TRAP_FRAME_X19_OFFSET
    SAVE_GPR s4,   TRAP_FRAME_X20_OFFSET
    SAVE_GPR s5,   TRAP_FRAME_X21_OFFSET
    SAVE_GPR s6,   TRAP_FRAME_X22_OFFSET
    SAVE_GPR s7,   TRAP_FRAME_X23_OFFSET
    SAVE_GPR s8,   TRAP_FRAME_X24_OFFSET
    SAVE_GPR s9,   TRAP_FRAME_X25_OFFSET
    SAVE_GPR s10,  TRAP_FRAME_X26_OFFSET
    SAVE_GPR s11,  TRAP_FRAME_X27_OFFSET
    SAVE_GPR t3,   TRAP_FRAME_X28_OFFSET
    SAVE_GPR t4,   TRAP_FRAME_X29_OFFSET
    SAVE_GPR t5,   TRAP_FRAME_X30_OFFSET
    SAVE_GPR t6,   TRAP_FRAME_X31_OFFSET

    SAVE_CSR mstatus, TRAP_FRAME_MSTATUS_OFFSET
    SAVE_CSR mepc,    TRAP_FRAME_MEPC_OFFSET
    SAVE_CSR mcause,  TRAP_FRAME_MCAUSE_OFFSET
    SAVE_CSR mtval,   TRAP_FRAME_MTVAL_OFFSET

    /* jixia_trap_dispatch(TrapFrame *frame) */
    mv   a0, sp
    call jixia_trap_dispatch

    /* Recoverable paths return here; fatal paths never return. */
    j jixia_trap_restore

    .size jixia_trap_entry, . - jixia_trap_entry

.balign 4
.global jixia_trap_restore
.type jixia_trap_restore, @function

/*
 * Restore a TrapFrame whose base address is the current sp, then return from
 * machine mode.
 */
jixia_trap_restore:
    RESTORE_CSR mstatus, TRAP_FRAME_MSTATUS_OFFSET
    RESTORE_CSR mepc,    TRAP_FRAME_MEPC_OFFSET

    RESTORE_GPR ra,   TRAP_FRAME_X1_OFFSET
    RESTORE_GPR gp,   TRAP_FRAME_X3_OFFSET
    RESTORE_GPR tp,   TRAP_FRAME_X4_OFFSET
    RESTORE_GPR t0,   TRAP_FRAME_X5_OFFSET
    RESTORE_GPR t1,   TRAP_FRAME_X6_OFFSET
    RESTORE_GPR t2,   TRAP_FRAME_X7_OFFSET
    RESTORE_GPR s0,   TRAP_FRAME_X8_OFFSET
    RESTORE_GPR s1,   TRAP_FRAME_X9_OFFSET
    RESTORE_GPR a0,   TRAP_FRAME_X10_OFFSET
    RESTORE_GPR a1,   TRAP_FRAME_X11_OFFSET
    RESTORE_GPR a2,   TRAP_FRAME_X12_OFFSET
    RESTORE_GPR a3,   TRAP_FRAME_X13_OFFSET
    RESTORE_GPR a4,   TRAP_FRAME_X14_OFFSET
    RESTORE_GPR a5,   TRAP_FRAME_X15_OFFSET
    RESTORE_GPR a6,   TRAP_FRAME_X16_OFFSET
    RESTORE_GPR a7,   TRAP_FRAME_X17_OFFSET
    RESTORE_GPR s2,   TRAP_FRAME_X18_OFFSET
    RESTORE_GPR s3,   TRAP_FRAME_X19_OFFSET
    RESTORE_GPR s4,   TRAP_FRAME_X20_OFFSET
    RESTORE_GPR s5,   TRAP_FRAME_X21_OFFSET
    RESTORE_GPR s6,   TRAP_FRAME_X22_OFFSET
    RESTORE_GPR s7,   TRAP_FRAME_X23_OFFSET
    RESTORE_GPR s8,   TRAP_FRAME_X24_OFFSET
    RESTORE_GPR s9,   TRAP_FRAME_X25_OFFSET
    RESTORE_GPR s10,  TRAP_FRAME_X26_OFFSET
    RESTORE_GPR s11,  TRAP_FRAME_X27_OFFSET
    RESTORE_GPR t3,   TRAP_FRAME_X28_OFFSET
    RESTORE_GPR t4,   TRAP_FRAME_X29_OFFSET
    RESTORE_GPR t5,   TRAP_FRAME_X30_OFFSET
    RESTORE_GPR t6,   TRAP_FRAME_X31_OFFSET

    /* No frame access is legal after restoring the interrupted sp. */
    RESTORE_GPR sp, TRAP_FRAME_X2_OFFSET
    mret

    .size jixia_trap_restore, . - jixia_trap_restore
