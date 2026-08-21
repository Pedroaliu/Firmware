#pragma once

#define JIXIA_TASK_SYSCALL_YIELD 0
#define JIXIA_TASK_SYSCALL_CREATE 1
#define JIXIA_TASK_SYSCALL_END 2
#define JIXIA_TASK_SYSCALL_WAIT 3
#define JIXIA_TASK_SYSCALL_DETACH 4
#define JIXIA_TASK_SYSCALL_SLEEP 5
#define JIXIA_TASK_SYSCALL_ENDPOINT_CREATE 6
#define JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY 7
#define JIXIA_TASK_SYSCALL_SEND 8
/*
 * Numbers 9 (ipc_call) and 12 (ipc_reply) are reserved by the M00-08.03 IPC
 * ABI and return -ENOSYS until the call/reply increments land. M00-08.03.02
 * implements number 10 as the blocking ipc_recv (syscall 11 stays the
 * non-blocking try_recv). The numbering itself is frozen here so later
 * increments never renumber the ABI.
 */
#define JIXIA_TASK_SYSCALL_CALL 9
#define JIXIA_TASK_SYSCALL_RECV 10
#define JIXIA_TASK_SYSCALL_TRY_RECV 11
#define JIXIA_TASK_SYSCALL_REPLY 12
#define JIXIA_TASK_SYSCALL_COUNT 13
