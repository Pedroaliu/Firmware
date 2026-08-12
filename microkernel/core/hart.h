//
// Created by pedroa on 2026/8/10.
//


#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/arch/riscv/hart_layout.h"
#include "microkernel/arch/riscv/hart_local_abi.h"


namespace jixia::microkernel::hart {

using HartId = uintptr_t;
using HartIndex = uint32_t;


enum class HartRole : uint32_t
{
    boot = 0,
    secondary = 1,
};


enum class HartState : uint32_t
{
    offline = 0,
    online = 1,
};


/*
 * Runtime state owned by one hart.
 *
 * 64-byte alignment is not required for correctness. It is an early
 * false-sharing avoidance choice for per-hart hot state.
 *
 * We do NOT yet claim that every target platform has a 64-byte cache line.
 */
struct alignas(64) HartLocal
{
    HartId hart_id;

    uintptr_t stack_bottom;
    uintptr_t stack_top;

    volatile uintptr_t machine_timer_interrupt_count;

    HartIndex index;
    HartRole role;

    /*
     * Accessed through compiler atomic builtins.
     *
     * Keep this as an integer field instead of std::atomic for now because
     * the current freestanding toolchain deliberately does not depend on the
     * hosted C++ standard library.
     */
    uint32_t state;

    uint32_t reserved;

    /*
     * Transient M-mode trap-entry scratch.
     *
     * RISC-V does not bank x2/sp across privilege levels. On every runtime
     * M-level trap, trap.S uses mscratch to reach this HartLocal, parks the
     * interrupted sp and t1 here without dereferencing the interrupted stack,
     * then switches to this hart's dedicated trusted trap stack.
     *
     * These fields are non-nestable scratch, not persistent execution state.
     */
    uintptr_t trap_entry_sp;
    uintptr_t trap_entry_t1;

    /*
     * Trusted stack used by every runtime trap handled in M-mode, regardless
     * of whether the interrupted context was M, S, or U.
     */
    uintptr_t trap_stack_bottom;
    uintptr_t trap_stack_top;

    /*
     * Nested M-level traps are deliberately unsupported in M00-06.
     * trap.S sets this before switching to the trap stack and fails closed if
     * another trap arrives before the first one completes.
     */
    uint32_t trap_active;
    uint32_t trap_reserved;
};


/*
 * trap.S accesses selected HartLocal fields by constant byte offset.
 * Keep the assembly/C++ ABI machine-checked just like TrapFrame.
 */
static_assert(
    offsetof(HartLocal, hart_id) ==
    HART_LOCAL_HART_ID_OFFSET);
static_assert(
    offsetof(HartLocal, stack_bottom) ==
    HART_LOCAL_STACK_BOTTOM_OFFSET);
static_assert(
    offsetof(HartLocal, stack_top) ==
    HART_LOCAL_STACK_TOP_OFFSET);
static_assert(
    offsetof(HartLocal, machine_timer_interrupt_count) ==
    HART_LOCAL_TIMER_COUNT_OFFSET);
static_assert(
    offsetof(HartLocal, index) ==
    HART_LOCAL_INDEX_OFFSET);
static_assert(
    offsetof(HartLocal, role) ==
    HART_LOCAL_ROLE_OFFSET);
static_assert(
    offsetof(HartLocal, state) ==
    HART_LOCAL_STATE_OFFSET);
static_assert(
    offsetof(HartLocal, reserved) ==
    HART_LOCAL_RESERVED_OFFSET);
static_assert(
    offsetof(HartLocal, trap_entry_sp) ==
    HART_LOCAL_TRAP_ENTRY_SP_OFFSET);
static_assert(
    offsetof(HartLocal, trap_entry_t1) ==
    HART_LOCAL_TRAP_ENTRY_T1_OFFSET);
static_assert(
    offsetof(HartLocal, trap_stack_bottom) ==
    HART_LOCAL_TRAP_STACK_BOTTOM_OFFSET);
static_assert(
    offsetof(HartLocal, trap_stack_top) ==
    HART_LOCAL_TRAP_STACK_TOP_OFFSET);
static_assert(
    offsetof(HartLocal, trap_active) ==
    HART_LOCAL_TRAP_ACTIVE_OFFSET);
static_assert(
    offsetof(HartLocal, trap_reserved) ==
    HART_LOCAL_TRAP_RESERVED_OFFSET);
static_assert(sizeof(HartLocal) == HART_LOCAL_SIZE);
static_assert(alignof(HartLocal) == 64U);


[[nodiscard]]
HartLocal& initialize(
    HartId hart_id,
    HartIndex index,
    HartRole role);


[[nodiscard]]
HartLocal& current();


[[nodiscard]]
const HartLocal* table();


[[nodiscard]]
bool all_online(HartIndex expected_count);


void wait_until_all_online(HartIndex expected_count);


[[noreturn]]
void park();


} // namespace jixia::microkernel::hart
