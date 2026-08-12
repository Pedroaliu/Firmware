#pragma once

#define HART_MAX_COUNT 4

#define HART_BOOT_STACK_SIZE 16384
#define HART_BOOT_STACK_ALIGNMENT 16

/*
 * Dedicated runtime M-mode trap stack per hart.
 *
 * Runtime traps from M/S/U all switch to this stack after HartLocal is bound.
 * Early bootstrap traps before HartLocal exists remain a separate policy.
 */
#define HART_TRAP_STACK_SIZE 4096
#define HART_TRAP_STACK_ALIGNMENT 16
