/*
 * M00-06.02: first controlled M -> S transition.
 *
 * This probe intentionally does not return from S-mode. The S -> M ECALL
 * round trip is the next submilestone (M00-06.03).
 */

.section .stack.harts.supervisor_test, "aw", @nobits

.balign 16
.global __m00_06_02_supervisor_stack_bottom
__m00_06_02_supervisor_stack_bottom:

    .space 4096

.global __m00_06_02_supervisor_stack_top
__m00_06_02_supervisor_stack_top:


.section .rodata.m00_06_02, "a", @progbits

.Lsupervisor_entry_message:
    .asciz "\n[Jixia][M00-06.02][Supervisor]\nM00_06_02_SUPERVISOR_ENTRY: PASS\n"


.section .text.m00_06_02, "ax", @progbits

.balign 4
.global jixia_m00_06_02_enter_supervisor
.type jixia_m00_06_02_enter_supervisor, @function

/*
 * Enter S-mode on the boot hart with translation and delegation disabled.
 *
 * The caller is ordinary M-mode C++ running on the normal per-hart M stack.
 * We keep that stack untouched, program the privilege transition while still
 * on trusted M storage, and let the S entry install its own stack immediately.
 */
jixia_m00_06_02_enter_supervisor:
    /* Eliminate asynchronous noise from the first transition experiment. */
    csrci mstatus, 8
    csrw mie, zero

    /* Keep all traps in M-mode for the first proof. */
    csrw medeleg, zero
    csrw mideleg, zero

    /* Bare address translation: physical addresses remain directly visible. */
    csrw satp, zero
    sfence.vma zero, zero

    /*
     * mstatus.MPP = S (01b).
     * Clear the two-bit field first, then set bit 11.
     */
    csrr t0, mstatus
    li t1, 3
    slli t1, t1, 11
    not t1, t1
    and t0, t0, t1

    li t1, 1
    slli t1, t1, 11
    or t0, t0, t1
    csrw mstatus, t0

    la t0, .Lsupervisor_entry
    csrw mepc, t0

    /* x2/sp is deliberately not changed in M-mode before mret. */
    mret


/*
 * First S-mode instructions.
 *
 * RISC-V does not bank x2/sp by privilege level, so immediately replace the
 * inherited M-mode x2 value with the dedicated S probe stack before making
 * any call or stack-using access.
 */
.Lsupervisor_entry:
    la sp, __m00_06_02_supervisor_stack_top
    andi sp, sp, -16

    /*
     * With satp=0 and no PMP isolation in this milestone, the QEMU UART is
     * directly reachable from S-mode. This marker is observability for the
     * transition proof, not a future supervisor console ABI.
     */
    la a0, .Lsupervisor_entry_message
    call uart_puts

.Lsupervisor_park:
    nop
    j .Lsupervisor_park

.size jixia_m00_06_02_enter_supervisor, . - jixia_m00_06_02_enter_supervisor
