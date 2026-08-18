#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/arch/riscv/isa.h"
#include "microkernel/arch/riscv/trap_frame.h"

namespace jixia::microkernel::task {

/**
 * Persistent execution state owned by a task while it is not running.
 *
 * TrapFrame is transient trusted trap-stack storage. TaskContext is durable
 * task-owned state and additionally carries the task's Sv39 identity.
 */
struct alignas(16) TaskContext {
    jixia::arch::riscv::Xlen x[32];
    jixia::arch::riscv::Xlen mstatus;
    jixia::arch::riscv::Xlen mepc;
    jixia::arch::riscv::Xlen satp;
};

static_assert(alignof(TaskContext) == 16U);
static_assert(offsetof(TaskContext, x) == 0U);
static_assert(offsetof(TaskContext, mstatus) == 256U);
static_assert(offsetof(TaskContext, mepc) == 264U);
static_assert(offsetof(TaskContext, satp) == 272U);
static_assert(sizeof(TaskContext) == 288U);

void clear(TaskContext& context);

void initialize_user(TaskContext& context, uintptr_t entry, uintptr_t argument, uintptr_t stack_top,
                     uintptr_t return_stub, uint64_t satp);

void save_from_trap(TaskContext& context, const jixia::arch::riscv::TrapFrame& frame);

void restore_to_trap(const TaskContext& context, jixia::arch::riscv::TrapFrame& frame);

} // namespace jixia::microkernel::task
