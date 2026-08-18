#pragma once

#include <stdint.h>

#include "microkernel/arch/riscv/sv39.h"
#include "microkernel/arch/riscv/trap_frame.h"

namespace jixia::microkernel::task {

using TaskId = uint64_t;

enum class TaskState : uint32_t {
    unused = 0,
    ready = 1,
    running = 2,
    exited = 3,
};

struct VSpace {
    jixia::arch::riscv::sv39::AddressSpace address_space;
    jixia::arch::riscv::sv39::Asid asid;
};

struct TaskContext {
    TaskId id;
    TaskState state;
    VSpace vspace;
    jixia::arch::riscv::TrapFrame user_frame;
};

struct UserTaskStart {
    uintptr_t entry_pc;
    uintptr_t stack_pointer;
    uintptr_t global_pointer;
    uintptr_t argument;
};

[[nodiscard]] bool initialize(TaskContext& context, TaskId id, const VSpace& vspace,
                              const UserTaskStart& start);

} // namespace jixia::microkernel::task