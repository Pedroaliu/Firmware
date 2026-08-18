#include "microkernel/arch/riscv/task_syscall_abi.h"
#include "microkernel/arch/riscv/user_task_test_values.h"

/*
 * First resident U-mode task code.
 *
 * The linker gives this code its own page-aligned section. VmmManager maps
 * only this section and task stacks into the bootstrap service VSpace.
 */
.section .user.text, "ax", @progbits

.balign 4
.global jixia_user_init_task
.type jixia_user_init_task, @function

jixia_user_init_task:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)

    /* Create one joinable child in the same bootstrap VSpace. */
    la a0, jixia_user_child_task
    li a1, JIXIA_M00_08_CHILD_ARGUMENT
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Linit_failure
    mv s0, a0

    /* Child is ahead of us in FIFO order, so yield switches to it. */
    li a7, JIXIA_TASK_SYSCALL_YIELD
    ecall

    /* Reap any completed child, matching Hostboot task_wait(-1). */
    li a0, -1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s0, .Linit_failure

    /* Create another child and block before that child has run. */
    la a0, jixia_user_child_task
    li a1, JIXIA_M00_08_BLOCKING_CHILD_ARGUMENT
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Linit_failure
    mv s1, a0

    mv a0, s1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s1, .Linit_failure

    /* Match Hostboot init/service behavior: no parent may join this daemon. */
    li a7, JIXIA_TASK_SYSCALL_DETACH
    ecall
    bnez a0, .Linit_failure

    li a0, JIXIA_M00_08_INIT_RESULT
    ld s1, 8(sp)
    ld s0, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

.Linit_failure:
    li a0, JIXIA_M00_08_INIT_FAILURE
    ld s1, 8(sp)
    ld s0, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

.size jixia_user_init_task, . - jixia_user_init_task

.balign 4
.global jixia_user_child_task
.type jixia_user_child_task, @function

jixia_user_child_task:
    /* Return the original a0 argument through the common end stub. */
    ret

.size jixia_user_child_task, . - jixia_user_child_task

.balign 4
.global jixia_user_task_end_stub
.type jixia_user_task_end_stub, @function

jixia_user_task_end_stub:
    li a7, JIXIA_TASK_SYSCALL_END
    ecall

.Ltask_end_returned:
    j .Ltask_end_returned

.size jixia_user_task_end_stub, . - jixia_user_task_end_stub

.balign 4
.global jixia_user_idle_task
.type jixia_user_idle_task, @function

jixia_user_idle_task:
    li a7, JIXIA_TASK_SYSCALL_YIELD
    ecall
    j jixia_user_idle_task

.size jixia_user_idle_task, . - jixia_user_idle_task
