#include "microkernel/arch/riscv/trap_frame.h"

.section .text.trap, "ax", @progbits

.balign 4
.global jixia_trap_entry
.type jixia_trap_entry, @function

.macro JIXIA_SAVE_GPR reg, offset
    sd \reg, \offset(sp)
.endm

.macro JIXIA_RESTORE_GPR reg, offset
    ld \reg, \offset(sp)
.endm

.macro JIXIA_SAVE_CSR csr, offset
    csrr t0, \csr
    sd   t0, \offset(sp)
.endm

.macro JIXIA_RESTORE_CSR csr, offset
    ld   t0, \offset(sp)
    csrw \csr, t0
.endm

/*
 * Save a complete RV64 integer context on the interrupted stack.
 *
 * M00-02 intentionally has no dedicated per-hart trap stack, stack guard,
 * or nesting policy yet. Interrupts remain disabled while this frame is live.
 */
jixia_trap_entry:
    addi sp, sp, -JIXIA_TRAP_FRAME_SIZE

    /* t0 must be preserved before it is used as the entry scratch register. */
    JIXIA_SAVE_GPR t0,   JIXIA_TRAP_X5_OFFSET

    JIXIA_SAVE_GPR zero, JIXIA_TRAP_X0_OFFSET
    JIXIA_SAVE_GPR ra,   JIXIA_TRAP_X1_OFFSET

    /* Recover and record the interrupted value of sp. */
    addi t0, sp, JIXIA_TRAP_FRAME_SIZE
    sd   t0, JIXIA_TRAP_X2_OFFSET(sp)

    JIXIA_SAVE_GPR gp,   JIXIA_TRAP_X3_OFFSET
    JIXIA_SAVE_GPR tp,   JIXIA_TRAP_X4_OFFSET
    JIXIA_SAVE_GPR t1,   JIXIA_TRAP_X6_OFFSET
    JIXIA_SAVE_GPR t2,   JIXIA_TRAP_X7_OFFSET
    JIXIA_SAVE_GPR s0,   JIXIA_TRAP_X8_OFFSET
    JIXIA_SAVE_GPR s1,   JIXIA_TRAP_X9_OFFSET
    JIXIA_SAVE_GPR a0,   JIXIA_TRAP_X10_OFFSET
    JIXIA_SAVE_GPR a1,   JIXIA_TRAP_X11_OFFSET
    JIXIA_SAVE_GPR a2,   JIXIA_TRAP_X12_OFFSET
    JIXIA_SAVE_GPR a3,   JIXIA_TRAP_X13_OFFSET
    JIXIA_SAVE_GPR a4,   JIXIA_TRAP_X14_OFFSET
    JIXIA_SAVE_GPR a5,   JIXIA_TRAP_X15_OFFSET
    JIXIA_SAVE_GPR a6,   JIXIA_TRAP_X16_OFFSET
    JIXIA_SAVE_GPR a7,   JIXIA_TRAP_X17_OFFSET
    JIXIA_SAVE_GPR s2,   JIXIA_TRAP_X18_OFFSET
    JIXIA_SAVE_GPR s3,   JIXIA_TRAP_X19_OFFSET
    JIXIA_SAVE_GPR s4,   JIXIA_TRAP_X20_OFFSET
    JIXIA_SAVE_GPR s5,   JIXIA_TRAP_X21_OFFSET
    JIXIA_SAVE_GPR s6,   JIXIA_TRAP_X22_OFFSET
    JIXIA_SAVE_GPR s7,   JIXIA_TRAP_X23_OFFSET
    JIXIA_SAVE_GPR s8,   JIXIA_TRAP_X24_OFFSET
    JIXIA_SAVE_GPR s9,   JIXIA_TRAP_X25_OFFSET
    JIXIA_SAVE_GPR s10,  JIXIA_TRAP_X26_OFFSET
    JIXIA_SAVE_GPR s11,  JIXIA_TRAP_X27_OFFSET
    JIXIA_SAVE_GPR t3,   JIXIA_TRAP_X28_OFFSET
    JIXIA_SAVE_GPR t4,   JIXIA_TRAP_X29_OFFSET
    JIXIA_SAVE_GPR t5,   JIXIA_TRAP_X30_OFFSET
    JIXIA_SAVE_GPR t6,   JIXIA_TRAP_X31_OFFSET

    JIXIA_SAVE_CSR mstatus, JIXIA_TRAP_MSTATUS_OFFSET
    JIXIA_SAVE_CSR mepc,    JIXIA_TRAP_MEPC_OFFSET
    JIXIA_SAVE_CSR mcause,  JIXIA_TRAP_MCAUSE_OFFSET
    JIXIA_SAVE_CSR mtval,   JIXIA_TRAP_MTVAL_OFFSET

    /* jixia_trap_dispatch(TrapFrame *frame) */
    mv   a0, sp
    call jixia_trap_dispatch

    /* M00-02 dispatch is fatal, but keep the restore path ABI-complete. */
    j jixia_trap_restore

    .size jixia_trap_entry, . - jixia_trap_entry

.balign 4
.global jixia_trap_restore
.type jixia_trap_restore, @function

/*
 * Restore a TrapFrame whose base address is the current sp, then return from
 * machine mode. M00-03 will make the C++ dispatch path return here normally.
 */
jixia_trap_restore:
    JIXIA_RESTORE_CSR mstatus, JIXIA_TRAP_MSTATUS_OFFSET
    JIXIA_RESTORE_CSR mepc,    JIXIA_TRAP_MEPC_OFFSET

    JIXIA_RESTORE_GPR ra,   JIXIA_TRAP_X1_OFFSET
    JIXIA_RESTORE_GPR gp,   JIXIA_TRAP_X3_OFFSET
    JIXIA_RESTORE_GPR tp,   JIXIA_TRAP_X4_OFFSET
    JIXIA_RESTORE_GPR t0,   JIXIA_TRAP_X5_OFFSET
    JIXIA_RESTORE_GPR t1,   JIXIA_TRAP_X6_OFFSET
    JIXIA_RESTORE_GPR t2,   JIXIA_TRAP_X7_OFFSET
    JIXIA_RESTORE_GPR s0,   JIXIA_TRAP_X8_OFFSET
    JIXIA_RESTORE_GPR s1,   JIXIA_TRAP_X9_OFFSET
    JIXIA_RESTORE_GPR a0,   JIXIA_TRAP_X10_OFFSET
    JIXIA_RESTORE_GPR a1,   JIXIA_TRAP_X11_OFFSET
    JIXIA_RESTORE_GPR a2,   JIXIA_TRAP_X12_OFFSET
    JIXIA_RESTORE_GPR a3,   JIXIA_TRAP_X13_OFFSET
    JIXIA_RESTORE_GPR a4,   JIXIA_TRAP_X14_OFFSET
    JIXIA_RESTORE_GPR a5,   JIXIA_TRAP_X15_OFFSET
    JIXIA_RESTORE_GPR a6,   JIXIA_TRAP_X16_OFFSET
    JIXIA_RESTORE_GPR a7,   JIXIA_TRAP_X17_OFFSET
    JIXIA_RESTORE_GPR s2,   JIXIA_TRAP_X18_OFFSET
    JIXIA_RESTORE_GPR s3,   JIXIA_TRAP_X19_OFFSET
    JIXIA_RESTORE_GPR s4,   JIXIA_TRAP_X20_OFFSET
    JIXIA_RESTORE_GPR s5,   JIXIA_TRAP_X21_OFFSET
    JIXIA_RESTORE_GPR s6,   JIXIA_TRAP_X22_OFFSET
    JIXIA_RESTORE_GPR s7,   JIXIA_TRAP_X23_OFFSET
    JIXIA_RESTORE_GPR s8,   JIXIA_TRAP_X24_OFFSET
    JIXIA_RESTORE_GPR s9,   JIXIA_TRAP_X25_OFFSET
    JIXIA_RESTORE_GPR s10,  JIXIA_TRAP_X26_OFFSET
    JIXIA_RESTORE_GPR s11,  JIXIA_TRAP_X27_OFFSET
    JIXIA_RESTORE_GPR t3,   JIXIA_TRAP_X28_OFFSET
    JIXIA_RESTORE_GPR t4,   JIXIA_TRAP_X29_OFFSET
    JIXIA_RESTORE_GPR t5,   JIXIA_TRAP_X30_OFFSET
    JIXIA_RESTORE_GPR t6,   JIXIA_TRAP_X31_OFFSET

    /* No frame access is legal after restoring the interrupted sp. */
    JIXIA_RESTORE_GPR sp, JIXIA_TRAP_X2_OFFSET
    mret

    .size jixia_trap_restore, . - jixia_trap_restore
