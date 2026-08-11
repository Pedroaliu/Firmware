#include "microkernel/arch/riscv/hart_local_abi.h"
#include "microkernel/arch/riscv/trap_frame.h"

/*
 * mstatus.MPP is a two-bit field at [12:11].
 *
 *   00 = U
 *   01 = S
 *   11 = M
 *
 * M00-06.01 only needs to distinguish M-origin traps from traps that
 * originated below M-mode. The latter must never construct privileged state
 * through the interrupted lower-privilege sp.
 */
#define MSTATUS_MPP_SHIFT 11


.section .text.trap, "ax", @progbits

.balign 4


/*
 * --------------------------------------------------------------------------
 * TrapFrame helpers
 * --------------------------------------------------------------------------
 *
 * Keep these macros local to trap.S for now. Their semantic contract is not
 * a generic push/pop ABI: they specifically assume that sp is the base of a
 * jixia::arch::riscv::TrapFrame.
 *
 * If a second assembly consumer later uses exactly this TrapFrame ABI, these
 * helpers can move to a dedicated trap_frame_asm.inc rather than a generic
 * common.inc.
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


/*
 * Save all GPRs except sp/t0/t1. Those three participate in the trusted-stack
 * entry protocol and are saved explicitly by each entry path.
 */
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


/*
 * A lower-privilege payload is free to use gp for its own ABI. Save that gp
 * first, then re-establish Jixia's global pointer before entering C++.
 */
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
 * mepc/mcause/mtval/mstatus. It has NOT saved the GPRs and it has NOT changed
 * sp.
 *
 * Security invariant:
 *
 *   a trap from below M-mode must never write privileged TrapFrame state via
 *   an interrupted sp controlled by that lower-privilege context.
 */
.global jixia_trap_entry
.type jixia_trap_entry, @function

jixia_trap_entry:
    /*
     * Obtain the per-hart trusted anchor without losing interrupted t0.
     *
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

    /*
     * Zero means hart::initialize() has not installed HartLocal yet. This is
     * valid only during early M-mode bootstrap, and that assumption is checked
     * below rather than trusted implicitly.
     */
    beqz t0, .Ltrap_anchor_not_installed

    /*
     * HartLocal exists. Preserve interrupted t1 in trusted per-hart storage
     * before using t1 to inspect mstatus.MPP.
     */
    sd t1, HART_LOCAL_TRAP_ENTRY_T1_OFFSET(t0)

    csrr t1, mstatus
    srli t1, t1, MSTATUS_MPP_SHIFT
    andi t1, t1, 3

    /* MPP == 3 means the interrupted context was already M-mode. */
    xori t1, t1, 3
    beqz t1, .Ltrap_from_machine

    /* U/S (and any reserved non-M value) use the lower-privilege path. */
    j .Ltrap_from_lower


/*
 * ==========================================================================
 * HartLocal installed, M -> M trap
 * ==========================================================================
 *
 * The interrupted sp is already a trusted Jixia M-mode stack. Undo the small
 * entry probe and preserve the existing M00-02/M00-03/M00-04 behavior.
 */
.Ltrap_from_machine:
    ld t1, HART_LOCAL_TRAP_ENTRY_T1_OFFSET(t0)

    /*
     * Before:
     *     t0       = HartLocal*
     *     mscratch = interrupted t0
     *
     * After:
     *     t0       = interrupted t0
     *     mscratch = HartLocal*
     */
    csrrw t0, mscratch, t0

    j .Lsave_on_current_machine_stack


/*
 * ==========================================================================
 * Lower privilege -> M trap
 * ==========================================================================
 *
 * The interrupted sp is untrusted. Record its VALUE only; never dereference
 * memory through it.
 */
.Ltrap_from_lower:
    sd sp, HART_LOCAL_TRAP_ENTRY_SP_OFFSET(t0)

    /* Switch to the trusted per-hart M-mode stack allocation. */
    ld sp, HART_LOCAL_STACK_TOP_OFFSET(t0)
    addi sp, sp, -TRAP_FRAME_SIZE

    /* Interrupted t0 was parked in mscratch by the first csrrw. */
    csrr t1, mscratch
    sd   t1, TRAP_FRAME_X5_OFFSET(sp)

    /* Interrupted t1 was parked in HartLocal. */
    ld t1, HART_LOCAL_TRAP_ENTRY_T1_OFFSET(t0)
    sd t1, TRAP_FRAME_X6_OFFSET(sp)

    /* Save the lower-privilege stack pointer itself, not memory through it. */
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
 * The intended state is early M-mode bootstrap. Prove that with MPP before
 * trusting the current stack. If lower privilege somehow traps here, fail
 * closed without touching memory through its sp.
 */
.Ltrap_anchor_not_installed:
    csrr t0, mstatus
    srli t0, t0, MSTATUS_MPP_SHIFT
    andi t0, t0, 3
    xori t0, t0, 3

    bnez t0, .Ltrap_no_trusted_anchor

    /*
     * MPP was M, so t0 is zero after xori. Exchange again to restore the
     * interrupted t0 and re-establish the explicit unbound mscratch == 0
     * state.
     */
    csrrw t0, mscratch, t0

    j .Lsave_on_current_machine_stack


/*
 * Lower-privilege trap with no trusted per-hart anchor. There is no safe C++
 * call or stack-based diagnostic path here yet, so stop using only registers.
 */
.Ltrap_no_trusted_anchor:
    csrw mscratch, zero
    csrci mstatus, 8
    csrw mie, zero

.Ltrap_no_trusted_anchor_halt:
    wfi
    j .Ltrap_no_trusted_anchor_halt


/*
 * ==========================================================================
 * Trusted current-stack save path
 * ==========================================================================
 *
 * All interrupted GPR values are live on entry here, and the current sp is an
 * M-mode-owned stack.
 */
.Lsave_on_current_machine_stack:
    addi sp, sp, -TRAP_FRAME_SIZE

    /* t0 must be saved before it becomes CSR scratch. */
    SAVE_GPR t0, TRAP_FRAME_X5_OFFSET

    /* Recover the interrupted M-mode sp. */
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

    /* The interrupted gp is already in TrapFrame. Restore the kernel ABI. */
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
     *
     * M-origin frame -> interrupted M sp
     * S/U-origin frame -> interrupted lower-privilege sp
     */
    RESTORE_GPR sp, TRAP_FRAME_X2_OFFSET

    /* mstatus.MPP and mepc select the actual privilege/PC return target. */
    mret

.size jixia_trap_restore, . - jixia_trap_restore
