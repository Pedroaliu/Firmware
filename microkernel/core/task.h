#pragma once

#include <stddef.h>
#include <stdint.h>

#include "microkernel/arch/riscv/sv39.h"
#include "microkernel/core/task_context.h"
#include "microkernel/core/task_types.h"

namespace jixia::microkernel::hart {
struct HartLocal;
}

namespace jixia::microkernel::task {

enum class TaskState : uint8_t {
    running = 'R',
    ready = 'r',
    ended = 'E',
    blocked_futex = 'f',
    blocked_message = 'M',
    blocked_userspace = 'u',
    blocked_sleep = 's',
    blocked_join = 'j',
};

enum class ExitStatus : int32_t {
    running = -1,
    exited_clean = 0,
    crashed = 1,
};

struct Task;

struct DelayNode {
    Task* previous;
    Task* next;
    uint64_t deadline;
    bool queued;
};

struct WaitInfo {
    bool active;
    int64_t tid;
    uintptr_t status_address;
    uintptr_t return_value_address;
};

struct TaskTracker {
    TaskTracker* parent;
    TaskTracker* first_child;
    TaskTracker* previous_sibling;
    TaskTracker* next_sibling;

    TaskId key;
    Task* task;
    ExitStatus status;
    uintptr_t return_value;
    WaitInfo wait;
    EntryPoint entry_point;
};

struct Task {
    hart::HartLocal* cpu;

    /* Keep context near the front and machine-check its location. */
    TaskContext context;
    jixia::arch::riscv::sv39::AddressSpace address_space;

    TaskId tid;
    uint64_t affinity_pinned;
    TaskState state;
    void* state_info;
    TaskTracker* tracker;

    /* Per-hart TimeManager queue state, mirroring Hostboot's delay_node. */
    DelayNode delay;

    uintptr_t stack_bottom;
    uintptr_t stack_top;

    bool detached;
    bool idle;
    bool queued;

    Task* previous;
    Task* next;
};

inline constexpr size_t kTaskContextOffset = 16U;
static_assert(offsetof(Task, context) == kTaskContextOffset);

} // namespace jixia::microkernel::task
