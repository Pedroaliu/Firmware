#include "microkernel/arch/riscv/pre_ddr_paging_test_values.h"

.section .stack.harts.pre_ddr_paging_test, "aw", @nobits

.balign 16
.global __m00_07_03_supervisor_stack_bottom
__m00_07_03_supervisor_stack_bottom:
    .space 4096
.global __m00_07_03_supervisor_stack_top
__m00_07_03_supervisor_stack_top:

.section .text.m00_07_03, "ax", @progbits

.balign 4
.global jixia_m00_07_03_enter_supervisor_paging
.type jixia_m00_07_03_enter_supervisor_paging, @function

/*
 * Input:
 *   a0 = complete Sv39 satp value prepared by resident M-mode firmware.
 */
jixia_m00_07_03_enter_supervisor_paging:
    /* Keep the paging proof deterministic and route S faults back to M. */
    csrci mstatus, 8
    csrw mie, zero
    csrw medeleg, zero
    csrw mideleg, zero

    /* The probe is about paging, not PMP isolation. */
    li t0, -1
    csrw pmpaddr0, t0
    li t0, 0x1f
    csrw pmpcfg0, t0

    csrw satp, a0
    sfence.vma zero, zero

    /* mstatus.MPP = S. */
    csrr t0, mstatus
    li t1, 3
    slli t1, t1, 11
    not t1, t1
    and t0, t0, t1
    li t1, 1
    slli t1, t1, 11
    or t0, t0, t1
    csrw mstatus, t0

    la t0, .Lsupervisor_paging_entry
    csrw mepc, t0
    mret

.Lsupervisor_paging_entry:
    la sp, __m00_07_03_supervisor_stack_top
    andi sp, sp, -16

    /*
     * 0x40000000 is deliberately unmapped. JALR commits ra before the target
     * instruction fetch, so after the M-mode pager maps the page, mret can
     * retry directly at the pageable target and RET returns here.
     */
    li t0, M00_07_03_PAGEABLE_VA
    jalr ra, t0, 0

.global jixia_m00_07_03_after_pageable_call
jixia_m00_07_03_after_pageable_call:
    li t0, M00_07_03_PAGEABLE_RESULT
    bne a0, t0, .Lpageable_result_fail

    li a0, M00_07_03_ECALL_PASS
.global jixia_m00_07_03_completion_ecall
jixia_m00_07_03_completion_ecall:
    ecall

.Lunexpected_return:
    j .Lunexpected_return

.Lpageable_result_fail:
    li a0, M00_07_03_ECALL_FAIL
.global jixia_m00_07_03_failure_ecall
jixia_m00_07_03_failure_ecall:
    ecall
    j .Lunexpected_return

.size jixia_m00_07_03_enter_supervisor_paging, . - jixia_m00_07_03_enter_supervisor_paging
