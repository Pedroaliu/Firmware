//
// Created by pedroa on 2026/8/10.
//


#pragma once

#include <stdint.h>

#include "microkernel/arch/riscv/hart_layout_defs.h"

namespace jixia::microkernel::hart {

inline constexpr uint32_t kMaxHarts =
    HART_MAX_COUNT;

inline constexpr uintptr_t kBootStackSize =
    HART_BOOT_STACK_SIZE;

inline constexpr uintptr_t kBootStackAlignment =
    HART_BOOT_STACK_ALIGNMENT;

inline constexpr uintptr_t kTrapStackSize =
    HART_TRAP_STACK_SIZE;

inline constexpr uintptr_t kTrapStackAlignment =
    HART_TRAP_STACK_ALIGNMENT;

} // namespace jixia::microkernel::hart
