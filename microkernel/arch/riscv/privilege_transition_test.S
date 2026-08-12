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
     * Give S-mode a deliberately permissive PMP window for this proof only.
     *
     * QEMU's rv64 CPU implements PMP. When PMP entries exist but do not match,
     * S/U accesses are not implicitly granted just because satp is bare. A
     * maximal NAPOT entry with RWX permissions lets the probe execute from the
     * firmware RAM image, use its S stack, and reach the QEMU UART MMIO.
     *
     * pmpcfg0[7:0] = 0x1f:
     *   R=1, W=1, X=1, A=NAPOT(11b), L=0
     *
     * This is NOT an isolation policy. PMP-backed service isolation is a later
     * milestone; M00-06.02 only isolates privilege-transition mechanics.
     */
    li t0, -1
    csrw pmpaddr0, t0

    li t0, 0x1f
    csrw pmpcfg0, t0

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
     * satp is bare and the temporary PMP entry permits this access. Direct
     * UART use is observability for this milestone, not a supervisor ABI.
     */
    la a0, .Lsupervisor_entry_message
    call uart_puts

.Lsupervisor_park:
    nop
    j .Lsupervisor_park

.size jixia_m00_06_02_enter_supervisor, . - jixia_m00_06_02_enter_supervisor
