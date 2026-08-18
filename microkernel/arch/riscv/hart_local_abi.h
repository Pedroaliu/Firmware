#pragma once

/*
 * Assembly/C++ ABI for the HartLocal fields required before a complete
 * TrapFrame exists.
 *
 * Keep these byte offsets numeric so preprocessed assembly can consume them.
 * C++ static_asserts in hart.h verify that the structure never drifts away
 * from this ABI silently.
 */

#define HART_LOCAL_HART_ID_OFFSET               0
#define HART_LOCAL_STACK_BOTTOM_OFFSET          8
#define HART_LOCAL_STACK_TOP_OFFSET             16
#define HART_LOCAL_TIMER_COUNT_OFFSET           24
#define HART_LOCAL_INDEX_OFFSET                 32
#define HART_LOCAL_ROLE_OFFSET                  36
#define HART_LOCAL_STATE_OFFSET                 40
#define HART_LOCAL_RESERVED_OFFSET              44
#define HART_LOCAL_TRAP_ENTRY_SP_OFFSET         48
#define HART_LOCAL_TRAP_ENTRY_T1_OFFSET         56
#define HART_LOCAL_TRAP_STACK_BOTTOM_OFFSET     64
#define HART_LOCAL_TRAP_STACK_TOP_OFFSET        72
#define HART_LOCAL_TRAP_ACTIVE_OFFSET           80
#define HART_LOCAL_TRAP_RESERVED_OFFSET         84
#define HART_LOCAL_CURRENT_TASK_OFFSET 88
#define HART_LOCAL_SCHEDULER_OFFSET 96
#define HART_LOCAL_SCHEDULER_EXTRA_OFFSET 104
#define HART_LOCAL_DELAY_LIST_OFFSET 112
#define HART_LOCAL_IDLE_TASK_OFFSET 120
#define HART_LOCAL_TIMESLICE_TICKS_OFFSET 128

/* alignas(64) rounds the 136-byte payload up to 192 bytes. */
#define HART_LOCAL_SIZE 192
