#include "microkernel/arch/riscv/hart_local_abi.h"
#include "microkernel/arch/riscv/trap_frame.h"

/* mstatus.MPP occupies bits [12:11]. */
#define MSTATUS_MPP_SHIFT 11


.section .text.trap, "ax", @progbits

.balign 4


/*
 * --------------------------------------------------------------------------
 * TrapFrame helpers
 * --------------------------------------------------------------------------
 *
 * These helpers assume sp is the base of a jixia::arch::riscv::TrapFrame.
 */
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


/* Save all GPRs except sp/t0/t1, which participate in entry switching. */
.macro SAVE_COMMON_GPRS
    SAVE_GPR zero, TRAP_FRAME_X0_OFFSET
    SAVE_GPR ra,   TRAP_FRAME_X1_OFFSET

    SAVE_GPR gp,   TRAP_FRAME_X3_OFFSET
    SAVE_GPR tp,   TRAP_FRAME_X4_OFFSET

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
.endm


/* Re-establish the firmware gp before calling C++ from any previous mode. */
.macro LOAD_KERNEL_GP
    .option push
    .option norelax

    la gp, __global_pointer$

    .option pop
.endm


/*
 * ==========================================================================
 * Machine trap entry
 * ==========================================================================
 *
 * RISC-V hardware has already entered M-mode and recorded trap metadata in
 * mepc/mcause/mtval/mstatus. It has NOT saved GPRs and it has NOT changed sp.
 *
 * Runtime policy after HartLocal is installed:
 *
 *   M-origin trap -> dedicated per-hart trusted M trap stack
 *   S-origin trap -> dedicated per-hart trusted M trap stack
 *   U-origin trap -> dedicated per-hart trusted M trap stack
 *
 * The interrupted x2/sp is always preserved as a VALUE before switching. It
 * is never dereferenced by trap entry. This keeps normal M execution, lower
 * privilege execution, and trap execution in separate stack domains.
 */
.global jixia_trap_entry
.type jixia_trap_entry, @function

jixia_trap_entry:
    /*
     * Normal bound state before:
     *
     *     t0       = interrupted t0
     *     mscratch = HartLocal*
     *
     * After:
     *
     *     t0       = HartLocal*
     *     mscratch = interrupted t0
     */
    csrrw t0, mscratch, t0

    /* Zero means hart::initialize() has not installed HartLocal yet. */
    beqz t0, .Ltrap_anchor_not_installed

    /*
     * Park values that must be preserved before t1/sp become entry scratch.
     * HartLocal is trusted per-hart M-mode state; the interrupted sp is not
     * dereferenced here even when the trap originated in M-mode.
     */
    sd t1, HART_LOCAL_TRAP_ENTRY_T1_OFFSET(t0)
    sd sp, HART_LOCAL_TRAP_ENTRY_SP_OFFSET(t0)

    /*
     * Nested/double traps are intentionally unsupported in M00-06. If a trap
     * occurs while a previous M-level trap is active, stop without touching
     * memory through the interrupted stack or overwriting the outer frame.
     */
    lw t1, HART_LOCAL_TRAP_ACTIVE_OFFSET(t0)
    bnez t1, .Ltrap_runtime_fail_closed

    li t1, 1
    sw t1, HART_LOCAL_TRAP_ACTIVE_OFFSET(t0)

    /* A bound HartLocal must have a valid dedicated trap stack. */
    ld t1, HART_LOCAL_TRAP_STACK_TOP_OFFSET(t0)
    beqz t1, .Ltrap_runtime_fail_closed

    mv sp, t1
    addi sp, sp, -TRAP_FRAME_SIZE

    /* Interrupted t0 was parked in mscratch by the first csrrw. */
    csrr t1, mscratch
    sd   t1, TRAP_FRAME_X5_OFFSET(sp)

    /* Interrupted t1 was parked in HartLocal. */
    ld t1, HART_LOCAL_TRAP_ENTRY_T1_OFFSET(t0)
    sd t1, TRAP_FRAME_X6_OFFSET(sp)

    /* Save interrupted x2/sp itself, not memory through it. */
    ld t1, HART_LOCAL_TRAP_ENTRY_SP_OFFSET(t0)
    sd t1, TRAP_FRAME_X2_OFFSET(sp)

    /* C++ handlers expect mscratch to identify current HartLocal. */
    csrw mscratch, t0

    SAVE_COMMON_GPRS

    j .Lsave_csrs_and_dispatch


/*
 * ==========================================================================
 * HartLocal anchor not installed
 * ==========================================================================
 *
 * At entry here:
 *
 *     t0       = 0
 *     mscratch = interrupted t0
 *
 * The only supported unbound state is early M-mode bootstrap. Prove MPP=M
 * before using the current stack. A lower-origin trap without a trusted
 * anchor fails closed using registers only.
 */
.Ltrap_anchor_not_installed:
    csrr t0, mstatus
    srli t0, t0, MSTATUS_MPP_SHIFT
    andi t0, t0, 3
    xori t0, t0, 3

    bnez t0, .Ltrap_no_trusted_anchor

    /* Restore interrupted t0 and explicit unbound mscratch == 0 state. */
    csrrw t0, mscratch, t0

    j .Lsave_on_early_machine_stack


.Ltrap_no_trusted_anchor:
    csrw mscratch, zero
    j .Ltrap_register_only_halt


/*
 * Runtime invariant failure: nested trap or missing trap-stack state.
 * Do not attempt stack-based diagnostics because stack ownership is no longer
 * trustworthy enough for recovery.
 */
.Ltrap_runtime_fail_closed:
    csrw mscratch, zero

.Ltrap_register_only_halt:
    csrci mstatus, 8
    csrw mie, zero

.Ltrap_register_only_halt_loop:
    wfi
    j .Ltrap_register_only_halt_loop


/*
 * ==========================================================================
 * Early bootstrap M -> M save path
 * ==========================================================================
 *
 * Runtime traps never use this path. It exists only for the explicit
 * pre-HartLocal bootstrap state, where the current M stack is the only trusted
 * storage available.
 */
.Lsave_on_early_machine_stack:
    addi sp, sp, -TRAP_FRAME_SIZE

    SAVE_GPR t0, TRAP_FRAME_X5_OFFSET

    addi t0, sp, TRAP_FRAME_SIZE
    sd   t0, TRAP_FRAME_X2_OFFSET(sp)

    SAVE_GPR t1, TRAP_FRAME_X6_OFFSET
    SAVE_COMMON_GPRS


/*
 * ==========================================================================
 * Common privileged save + C++ dispatch
 * ==========================================================================
 */
.Lsave_csrs_and_dispatch:
    SAVE_CSR mstatus, TRAP_FRAME_MSTATUS_OFFSET
    SAVE_CSR mepc,    TRAP_FRAME_MEPC_OFFSET
    SAVE_CSR mcause,  TRAP_FRAME_MCAUSE_OFFSET
    SAVE_CSR mtval,   TRAP_FRAME_MTVAL_OFFSET

    /* Interrupted gp is already in TrapFrame. Restore the firmware ABI. */
    LOAD_KERNEL_GP

    /* jixia_trap_dispatch(TrapFrame *frame) */
    mv a0, sp
    call jixia_trap_dispatch

    /* Recoverable paths return here; fatal paths never return. */
    j jixia_trap_restore

.size jixia_trap_entry, . - jixia_trap_entry


/*
 * ==========================================================================
 * Trap restore
 * ==========================================================================
 */
.balign 4
.global jixia_trap_restore
.type jixia_trap_restore, @function

jixia_trap_restore:
    /*
     * Runtime entry marked trap_active before using the dedicated trap stack.
     * Early bootstrap has mscratch == 0 and skips this bookkeeping.
     *
     * t0 is safe scratch here because its interrupted value remains in the
     * TrapFrame until RESTORE_GPR t0 below.
     */
    csrr t0, mscratch
    beqz t0, .Ltrap_restore_csrs
    sw zero, HART_LOCAL_TRAP_ACTIVE_OFFSET(t0)

.Ltrap_restore_csrs:
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

    /*
     * No TrapFrame access is legal after restoring sp.
     * TrapFrame.x2 always contains the interrupted x2/sp value.
     */
    RESTORE_GPR sp, TRAP_FRAME_X2_OFFSET

    /* mstatus.MPP and mepc select the actual privilege/PC return target. */
    mret

.size jixia_trap_restore, . - jixia_trap_restore
