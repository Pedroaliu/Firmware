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

#ifdef JIXIA_M00_08_03_01_PROBE

/*
 * M00-08.03.01 non-blocking IPC acceptance:
 *
 *   endpoint create/destroy lifecycle with generation isolation (C15)
 *   send-before-recv persistence (C01), FIFO ordering (C03)
 *   malformed and stale handles fail closed with -EINVAL (C14)
 *   queue-full -EAGAIN and post-drain recovery (C16)
 *   reserved blocking syscalls fail closed with -ENOSYS
 */
.balign 4
.global jixia_user_ipc_init_task
.type jixia_user_ipc_init_task, @function

jixia_user_ipc_init_task:
    /* 15 endpoint handles for the table-capacity case live at 0(sp)..112(sp). */
    addi sp, sp, -160
    sd ra, 152(sp)
    sd s0, 144(sp)
    sd s1, 136(sp)
    sd s2, 128(sp)

    /* 1. Bootstrap endpoint: typed handle, generation never zero. */
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    beqz a0, .Lipc_init_failure
    mv s0, a0

    /*
     * 2. C01: send strictly before any receive attempt exists. Four distinct
     * payload words exercise the full a1..a4 send/recv register ABI.
     */
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C01_W0
    li a2, JIXIA_M00_08_IPC_WORD_C01_W1
    li a3, JIXIA_M00_08_IPC_WORD_C01_W2
    li a4, JIXIA_M00_08_IPC_WORD_C01_W3
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    /* A second sender task queues its own four-word payload next (FIFO). */
    la a0, jixia_user_ipc_sender_child
    mv a1, s0
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s1, a0

    la a0, jixia_user_ipc_receiver_child
    mv a1, s0
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s2, a0

    li a7, JIXIA_TASK_SYSCALL_YIELD
    ecall

    mv a0, s1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s1, .Lipc_init_failure

    mv a0, s2
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s2, .Lipc_init_failure

    /* 3. C03: three sends; the consumer child checks FIFO order. */
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C03_1
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C03_2
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C03_3
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    la a0, jixia_user_ipc_fifo_child
    mv a1, s0
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s1, a0

    li a7, JIXIA_TASK_SYSCALL_YIELD
    ecall

    mv a0, s1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s1, .Lipc_init_failure

    /* 4. C16: fill the 16-deep FIFO; the 17th send must be -EAGAIN. */
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C16_FIRST
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    li s2, 0
.Lipc_c16_fill:
    li t0, 15
    bge s2, t0, .Lipc_c16_full_probe
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C16_REST
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure
    addi s2, s2, 1
    j .Lipc_c16_fill

.Lipc_c16_full_probe:
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C16_REST
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    li t0, -11
    bne a0, t0, .Lipc_init_failure

    /* Drain one message: the oldest (first-filled) word must survive. */
    mv a0, s0
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_init_failure
    li t0, JIXIA_M00_08_IPC_WORD_C16_FIRST
    bne a1, t0, .Lipc_init_failure

    /* Recovery: the freed slot accepts one more send. */
    mv a0, s0
    li a1, JIXIA_M00_08_IPC_WORD_C16_REST
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

.Lipc_init_continue_1:

    /* 5. C14: malformed handles fail closed with -EINVAL everywhere. */
    li a0, JIXIA_M00_08_IPC_GARBAGE_HANDLE
    li a1, JIXIA_M00_08_IPC_WORD_C01_W0
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    li a0, JIXIA_M00_08_IPC_GARBAGE_HANDLE
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    li a0, JIXIA_M00_08_IPC_GARBAGE_HANDLE
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    li a0, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    /* Bit 63 set (generation 0x80000000): never a legal handle value. */
    li a0, JIXIA_M00_08_IPC_BIT63_HANDLE
    li a1, JIXIA_M00_08_IPC_WORD_C01_W0
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    li a0, JIXIA_M00_08_IPC_BIT63_HANDLE
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    /*
     * 6. C14b: destroy/recreate generation isolation on a recycled slot.
     * (Renamed from the first-pass "C15" label; the C15 id belongs to the
     * try_recv-never-blocks case in the research acceptance plan.)
     */
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s1, a0

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    bnez a0, .Lipc_init_failure

    /* Stale handle from the destroyed epoch: every operation -EINVAL. */
    mv a0, s1
    li a1, JIXIA_M00_08_IPC_WORD_C01_W0
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    li t0, -22
    bne a0, t0, .Lipc_init_failure

    /* Recreate: same slot index, generation advanced by exactly one epoch. */
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    li t0, 0x100000000
    add t0, s1, t0
    bne a0, t0, .Lipc_init_failure
    mv s1, a0

    /* The new epoch works while the old alias above stays rejected. */
    mv a0, s1
    li a1, JIXIA_M00_08_IPC_WORD_C14B
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_init_failure
    li t0, JIXIA_M00_08_IPC_WORD_C14B
    bne a1, t0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    bnez a0, .Lipc_init_failure

    /*
     * 7. C15 (research acceptance plan): try_recv never blocks. An empty
     * endpoint returns -EAGAIN (= -EWOULDBLOCK) immediately; on this
     * single-hart deterministic workload the marker order itself proves each
     * syscall returned (a blocking try_recv would hang the boot and the
     * summary marker would never appear).
     */
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s1, a0

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    li t0, -11
    bne a0, t0, .Lipc_init_failure

    mv a0, s1
    li a1, JIXIA_M00_08_IPC_WORD_C15
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_init_failure
    li t0, JIXIA_M00_08_IPC_WORD_C15
    bne a1, t0, .Lipc_init_failure

    /* Drained again: still an immediate -EAGAIN, never a block. */
    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    li t0, -11
    bne a0, t0, .Lipc_init_failure

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    bnez a0, .Lipc_init_failure

    /* 8. Non-owner destroy must be denied with -EACCES (child asserts). */
    la a0, jixia_user_ipc_nonowner_child
    mv a1, s0
    li a7, JIXIA_TASK_SYSCALL_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    mv s1, a0

    /* Wait before the child runs: exercises the blocking wait + idle path. */
    mv a0, s1
    li a1, 0
    li a2, 0
    li a7, JIXIA_TASK_SYSCALL_WAIT
    ecall
    bne a0, s1, .Lipc_init_failure

    /* 9. Reserved blocking syscall fails closed with -ENOSYS. */
    li a0, 0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    li a7, JIXIA_TASK_SYSCALL_CALL
    ecall
    li t0, -38
    bne a0, t0, .Lipc_init_failure

    /* 10. Table capacity: 16 endpoints total, the 17th create is -ENOSPC. */
    li s2, 0
.Lipc_fill_table:
    li t0, 15
    bge s2, t0, .Lipc_table_full_probe
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    bltz a0, .Lipc_init_failure
    slli t1, s2, 3
    add t1, t1, sp
    sd a0, 0(t1)
    addi s2, s2, 1
    j .Lipc_fill_table

.Lipc_table_full_probe:
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_CREATE
    ecall
    li t0, -28
    bne a0, t0, .Lipc_init_failure

    li s2, 0
.Lipc_drain_table:
    li t0, 15
    bge s2, t0, .Lipc_final_cleanup
    slli t1, s2, 3
    add t1, t1, sp
    ld a0, 0(t1)
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    bnez a0, .Lipc_init_failure
    addi s2, s2, 1
    j .Lipc_drain_table

.Lipc_final_cleanup:
    mv a0, s0
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    bnez a0, .Lipc_init_failure

    li a7, JIXIA_TASK_SYSCALL_DETACH
    ecall
    bnez a0, .Lipc_init_failure

    li a0, JIXIA_M00_08_IPC_INIT_RESULT
    j .Lipc_init_return

.Lipc_init_failure:
    li a0, JIXIA_M00_08_IPC_INIT_FAILURE

.Lipc_init_return:
    ld s2, 128(sp)
    ld s1, 136(sp)
    ld s0, 144(sp)
    ld ra, 152(sp)
    addi sp, sp, 160
    ret

.size jixia_user_ipc_init_task, . - jixia_user_ipc_init_task

/*
 * C01 second sender: a distinct task queues its own four-word payload so the
 * receiver can prove the a5 sender-TaskId return tracks the real sender.
 */
.balign 4
.global jixia_user_ipc_sender_child
.type jixia_user_ipc_sender_child, @function

jixia_user_ipc_sender_child:
    /* a0 = endpoint handle created by the init task. */
    li a1, JIXIA_M00_08_IPC_WORD_C01B_W0
    li a2, JIXIA_M00_08_IPC_WORD_C01B_W1
    li a3, JIXIA_M00_08_IPC_WORD_C01B_W2
    li a4, JIXIA_M00_08_IPC_WORD_C01B_W3
    li a7, JIXIA_TASK_SYSCALL_SEND
    ecall
    bnez a0, .Lipc_sender_failure
    li a0, JIXIA_M00_08_IPC_SENDER_CHILD_RESULT
    ret

.Lipc_sender_failure:
    li a0, JIXIA_M00_08_IPC_INIT_FAILURE
    ret

.size jixia_user_ipc_sender_child, . - jixia_user_ipc_sender_child

/*
 * C01 consumer: pops both messages. All four payload registers a1..a4 must
 * carry the exact words each sender wrote, and a5 (sender TaskId) must change
 * between the two pops — the second message came from a different task.
 */
.balign 4
.global jixia_user_ipc_receiver_child
.type jixia_user_ipc_receiver_child, @function

jixia_user_ipc_receiver_child:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01_W0
    bne a1, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01_W1
    bne a2, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01_W2
    bne a3, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01_W3
    bne a4, t0, .Lipc_receiver_failure
    /* Remember the first sender's TaskId (the init task). */
    mv s0, a5

    mv a0, s1
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01B_W0
    bne a1, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01B_W1
    bne a2, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01B_W2
    bne a3, t0, .Lipc_receiver_failure
    li t0, JIXIA_M00_08_IPC_WORD_C01B_W3
    bne a4, t0, .Lipc_receiver_failure
    /* Sender identity must track the real sender: a different task now. */
    beq a5, s0, .Lipc_receiver_failure

    li a0, JIXIA_M00_08_IPC_RECEIVER_CHILD_RESULT
    j .Lipc_receiver_return

.Lipc_receiver_failure:
    li a0, JIXIA_M00_08_IPC_INIT_FAILURE

.Lipc_receiver_return:
    ld s1, 8(sp)
    ld s0, 16(sp)
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

.size jixia_user_ipc_receiver_child, . - jixia_user_ipc_receiver_child

/* C03 consumer: three pops must arrive in send order. */
.balign 4
.global jixia_user_ipc_fifo_child
.type jixia_user_ipc_fifo_child, @function

jixia_user_ipc_fifo_child:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0

    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_fifo_failure
    li t0, JIXIA_M00_08_IPC_WORD_C03_1
    bne a1, t0, .Lipc_fifo_failure

    mv a0, s0
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_fifo_failure
    li t0, JIXIA_M00_08_IPC_WORD_C03_2
    bne a1, t0, .Lipc_fifo_failure

    mv a0, s0
    li a7, JIXIA_TASK_SYSCALL_TRY_RECV
    ecall
    bnez a0, .Lipc_fifo_failure
    li t0, JIXIA_M00_08_IPC_WORD_C03_3
    bne a1, t0, .Lipc_fifo_failure

    li a0, JIXIA_M00_08_IPC_FIFO_CHILD_RESULT
    j .Lipc_fifo_return

.Lipc_fifo_failure:
    li a0, JIXIA_M00_08_IPC_INIT_FAILURE

.Lipc_fifo_return:
    ld s0, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

.size jixia_user_ipc_fifo_child, . - jixia_user_ipc_fifo_child

/* Destroy of a foreign endpoint must be denied with -EACCES. */
.balign 4
.global jixia_user_ipc_nonowner_child
.type jixia_user_ipc_nonowner_child, @function

jixia_user_ipc_nonowner_child:
    /* a0 = endpoint owned by the init task. */
    li a7, JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY
    ecall
    li t0, -13
    bne a0, t0, .Lipc_nonowner_failure
    li a0, JIXIA_M00_08_IPC_NONOWNER_CHILD_RESULT
    ret

.Lipc_nonowner_failure:
    li a0, JIXIA_M00_08_IPC_INIT_FAILURE
    ret

.size jixia_user_ipc_nonowner_child, . - jixia_user_ipc_nonowner_child

#endif
