#include "microkernel/arch/riscv/trap_frame.h"

.section .text.task_dispatch, "ax", @progbits

.balign 4
.global jixia_task_enter_first
.type jixia_task_enter_first, @function

/*
 * a0 = prepared TrapFrame for the selected task
 * a1 = task satp value
 *
 * Reuse the common TrapFrame restore path so first dispatch and every later
 * trap return share one register/privilege ABI.
 */
jixia_task_enter_first:
    csrw satp, a1
    sfence.vma zero, zero
    fence.i

    mv sp, a0
    j jixia_trap_restore

.size jixia_task_enter_first, . - jixia_task_enter_first
