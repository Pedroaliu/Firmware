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

/* alignas(64) rounds the 88-byte payload up to 128 bytes. */
#define HART_LOCAL_SIZE                         128
