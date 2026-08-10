//
// Created by pedroa on 2026/8/10.
//


#pragma once

#include <stdint.h>

#include "microkernel/arch/riscv/hart_layout.h"


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
};


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
bool all_online();


void wait_until_all_online();


[[noreturn]]
void park();


} // namespace jixia::microkernel::hart