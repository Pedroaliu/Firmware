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

#ifdef JIXIA_M00_08_02_PROBE

/*
 * Hostboot-shaped preemption acceptance:
 *
 *   init -> CPU-bound child (never yields) -> witness child
 *        -> timer must preempt the CPU-bound child so witness can run
 *        -> sleeper blocks -> idle deadline wakes sleeper -> init resumes
 */
.balign 4
.global jixia_user_preemption_init_task
.type jixia_user_preemption_init_task, @function

jixia_user_preemption_init_task:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)

    /* Both children share this mapped U-stack word as a one-shot handshake. */
    sd zero, 0(sp)
    mv s3, sp

    la a0, jixia_user_cpu_bound_task
    mv a1, s3
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lpreemption_init_failure
    mv s0, a0

    la a0, jixia_user_preemption_witness_task
    mv a1, s3
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lpreemption_init_failure
    mv s1, a0

    /* FIFO selects the non-cooperative child first. Only mtime can stop it. */
    li a7, JIXIA_TASK_SYSCALL_YIELD
    ecall

    mv a0, s1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s1, .Lpreemption_init_failure

    /* The witness releases the CPU-bound child, which is then reclaimed. */
    mv a0, s0
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s0, .Lpreemption_init_failure

    la a0, jixia_user_sleep_task
    li a1, JIXIA_M00_08_SLEEP_RESULT
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lpreemption_init_failure
    mv s2, a0

    mv a0, s2
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s2, .Lpreemption_init_failure

    li a7, JIXIA_TASK_SYSCALL_DETACH
    ecall
    bnez a0, .Lpreemption_init_failure

    li a0, JIXIA_M00_08_PREEMPT_INIT_RESULT
    j .Lpreemption_init_return

.Lpreemption_init_failure:
    li a0, JIXIA_M00_08_PREEMPT_INIT_FAILURE

.Lpreemption_init_return:
    ld s3, 24(sp)
    ld s2, 32(sp)
    ld s1, 40(sp)
    ld s0, 48(sp)
    ld ra, 56(sp)
    addi sp, sp, 64
    ret

.size jixia_user_preemption_init_task, . - jixia_user_preemption_init_task

.balign 4
.global jixia_user_cpu_bound_task
.type jixia_user_cpu_bound_task, @function

jixia_user_cpu_bound_task:
    /* No syscall and no finite exit: only the witness can release this loop. */
.Lcpu_bound_loop:
    ld t0, 0(a0)
    beqz t0, .Lcpu_bound_loop
    li a0, JIXIA_M00_08_PREEMPT_HOG_RESULT
    ret

.size jixia_user_cpu_bound_task, . - jixia_user_cpu_bound_task

.balign 4
.global jixia_user_preemption_witness_task
.type jixia_user_preemption_witness_task, @function

jixia_user_preemption_witness_task:
    li t0, 1
    sd t0, 0(a0)
    li a0, JIXIA_M00_08_PREEMPT_WITNESS_RESULT
    ret

.size jixia_user_preemption_witness_task, . - jixia_user_preemption_witness_task

.balign 4
.global jixia_user_sleep_task
.type jixia_user_sleep_task, @function

jixia_user_sleep_task:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0

    li a0, 0
    li a1, 2000000
    li a7, JIXIA_TASK_SYSCALL_SLEEP
    ecall
    bnez a0, .Lsleep_failure

    mv a0, s0
    j .Lsleep_return

.Lsleep_failure:
    li a0, JIXIA_M00_08_PREEMPT_INIT_FAILURE

.Lsleep_return:
    ld s0, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

.size jixia_user_sleep_task, . - jixia_user_sleep_task

#endif
