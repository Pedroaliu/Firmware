#pragma once

#include <stdint.h>

namespace jixia::microkernel::timer {

void arm_once(uint64_t delta_ticks);
void arm_task_deadline(uint64_t deadline);
void handle_interrupt();
void disable_global_interrupts();

[[nodiscard]] uintptr_t interrupt_count();

} // namespace jixia::microkernel::timer
