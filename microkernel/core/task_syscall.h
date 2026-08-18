#pragma once

#include "microkernel/arch/riscv/trap_frame.h"

namespace jixia::microkernel::task::syscall {

[[nodiscard]] bool try_handle(jixia::arch::riscv::TrapFrame& frame);

} // namespace jixia::microkernel::task::syscall
