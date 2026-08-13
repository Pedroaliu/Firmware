#include "microkernel/arch/riscv/privilege_boundary_test_values.h"

/*
 * M00-06.04: hostile lower-privilege x2/sp and no-anchor fail-closed probe.
 *
 * The first S -> M -> S ECALL deliberately uses an unmapped x2/sp value.
 * Machine trap entry must preserve that value without dereferencing it, switch
 * to HartLocal.trap_stack_top, and construct the TrapFrame only on trusted M
 * storage. The M-side probe handler then clears the trusted mscratch anchor
 * before returning so a second S-mode ECALL proves the no-anchor lower-origin
 * path fails closed in registers only.
 */

.section .stack.harts.supervisor_boundary_test, "aw", @nobits

.balign 16
.global __m00_06_04_supervisor_stack_bottom
__m00_06_04_supervisor_stack_bottom:

    .space 4096

.global __m00_06_04_supervisor_stack_top
__m00_06_04_supervisor_stack_top:


.section .rodata.m00_06_04, "a", @progbits

.Lboundary_entry_message:
    .asciz "\n[Jixia][M00-06.04][Supervisor]\nM00_06_04_HOSTILE_SP_ENTRY: PASS\n"

.Lhostile_return_message:
    .asciz "\n[Jixia][M00-06.04][Supervisor]\nM00_06_04_HOSTILE_SP_RETURN: PASS\n"

.Lhostile_return_fail_message:
    .asciz "\n[Jixia][M00-06.04][Supervisor]\nM00_06_04_HOSTILE_SP_RETURN: FAIL\n"

.Lno_anchor_armed_message:
    .asciz "M00_06_04_NO_ANCHOR_ARMED: PASS\n"

.Lno_anchor_return_fail_message:
    .asciz "M00_06_04_NO_ANCHOR_RETURN: FAIL\n"


.section .text.m00_06_04, "ax", @progbits

.balign 4
.global jixia_m00_06_04_enter_supervisor_boundary
.type jixia_m00_06_04_enter_supervisor_boundary, @function

jixia_m00_06_04_enter_supervisor_boundary:
    /* Keep the boundary proof deterministic and route all traps to M-mode. */
    csrci mstatus, 8
    csrw mie, zero
    csrw medeleg, zero
    csrw mideleg, zero

    /* Paging remains out of scope for M00-06. */
    csrw satp, zero
    sfence.vma zero, zero

    /* Reuse the deliberately permissive bootstrap PMP probe permission. */
    li t0, -1
    csrw pmpaddr0, t0

    li t0, 0x1f
    csrw pmpcfg0, t0

    /* mstatus.MPP = S (01b). */
    csrr t0, mstatus
    li t1, 3
    slli t1, t1, 11
    not t1, t1
    and t0, t0, t1

    li t1, 1
    slli t1, t1, 11
    or t0, t0, t1
    csrw mstatus, t0

    la t0, .Lboundary_supervisor_entry
    csrw mepc, t0

    mret


.Lboundary_supervisor_entry:
    /* Establish valid S storage before any call or stack-using instruction. */
    la sp, __m00_06_04_supervisor_stack_top
    andi sp, sp, -16

    la a0, .Lboundary_entry_message
    call uart_puts

    /* Known lower-privilege context must survive the hostile-stack ECALL. */
    li gp, M00_06_04_GP_MARKER
    li a0, M00_06_04_A0_MARKER
    li a7, M00_06_04_A7_MARKER

    /*
     * Deliberately destroy lower-privilege stack trust. No instruction between
     * this write and ECALL may require stack storage.
     */
    li sp, M00_06_04_HOSTILE_SP_MARKER

.global jixia_m00_06_04_hostile_sp_ecall_site
jixia_m00_06_04_hostile_sp_ecall_site:
    ecall

.global jixia_m00_06_04_after_hostile_sp_ecall
jixia_m00_06_04_after_hostile_sp_ecall:
    /* Still no stack access: first prove x2 and selected context were restored. */
    li t0, M00_06_04_HOSTILE_SP_MARKER
    bne sp, t0, .Lhostile_return_fail

    li t0, M00_06_04_GP_MARKER
    bne gp, t0, .Lhostile_return_fail

    li t0, M00_06_04_A0_MARKER
    bne a0, t0, .Lhostile_return_fail

    li t0, M00_06_04_A7_MARKER
    bne a7, t0, .Lhostile_return_fail

    /* Restore valid S storage before returning to normal call conventions. */
    .option push
    .option norelax
    la sp, __m00_06_04_supervisor_stack_top
    la gp, __global_pointer$
    .option pop
    andi sp, sp, -16

    la a0, .Lhostile_return_message
    call uart_puts

    /*
     * The M-side handler intentionally removed mscratch after validating the
     * first ECALL. The second ECALL therefore originates in S with no trusted
     * HartLocal anchor. trap.S must enter the register-only fail-closed loop;
     * this instruction must never return.
     */
    la a0, .Lno_anchor_armed_message
    call uart_puts

.global jixia_m00_06_04_no_anchor_ecall_site
jixia_m00_06_04_no_anchor_ecall_site:
    ecall

    /* Reaching here means the no-anchor lower-origin boundary failed open. */
    la a0, .Lno_anchor_return_fail_message
    call uart_puts
    j .Lboundary_park


.Lhostile_return_fail:
    /* Repair S ABI state before emitting a diagnostic marker. */
    .option push
    .option norelax
    la sp, __m00_06_04_supervisor_stack_top
    la gp, __global_pointer$
    .option pop
    andi sp, sp, -16

    la a0, .Lhostile_return_fail_message
    call uart_puts


.Lboundary_park:
    nop
    j .Lboundary_park

.size jixia_m00_06_04_enter_supervisor_boundary, . - jixia_m00_06_04_enter_supervisor_boundary
