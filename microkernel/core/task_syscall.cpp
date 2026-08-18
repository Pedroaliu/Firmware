#include "microkernel/core/task_syscall.h"

#include <stdint.h>

#include "microkernel/arch/riscv/task_syscall_abi.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/user_task_test_values.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/scheduler.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/vmm_manager.h"

namespace jixia::microkernel::task::syscall {
namespace {

constexpr uintptr_t kMstatusMppMask = uintptr_t{3U} << 11U;
constexpr intptr_t kErrorNoMemory = -12;
constexpr intptr_t kErrorDeadlock = -35;
constexpr intptr_t kErrorNotSupported = -95;

bool g_create_reported = false;
bool g_context_switch_reported = false;
bool g_end_reported = false;
bool g_wait_reported = false;
bool g_wait_block_reported = false;
bool g_detach_reported = false;
bool g_idle_reported = false;

[[nodiscard]] uintptr_t error_value(intptr_t error) {
    return static_cast<uintptr_t>(error);
}

} // namespace

bool try_handle(jixia::arch::riscv::TrapFrame& frame) {
    const jixia::arch::riscv::TrapCause cause{frame.mcause};
    if (!cause.is_exception(jixia::arch::riscv::ExceptionCode::environment_call_from_u) ||
        (frame.mstatus & kMstatusMppMask) != 0U) {
        return false;
    }

    Task* caller = TaskManager::get_current_task();
    if (caller == nullptr) {
        return false;
    }

    TaskManager::save_current_context(frame);
    caller->context.mepc += 4U;

    const uintptr_t number = caller->context.x[17];
    switch (number) {
    case JIXIA_TASK_SYSCALL_YIELD: {
        const TaskId caller_tid = caller->tid;
        caller->context.x[10] = 0U;
        caller->cpu->scheduler->return_runnable();
        Task& selected = caller->cpu->scheduler->set_next_runnable();

        if (caller->idle && !g_idle_reported) {
            printk("M00_08_IDLE_TASK: PASS\n");
            g_idle_reported = true;
        } else if (selected.tid != caller_tid && !g_context_switch_reported) {
            printk("M00_08_TASK_YIELD: PASS\n"
                   "M00_08_CONTEXT_SWITCH: PASS\n");
            g_context_switch_reported = true;
        }
        break;
    }

    case JIXIA_TASK_SYSCALL_CREATE: {
        const uintptr_t entry = caller->context.x[10];
        const uintptr_t argument = caller->context.x[11];

        if (!memory::VmmManager::instance().is_boot_user_entry(entry)) {
            caller->context.x[10] = error_value(kErrorNotSupported);
            break;
        }

        Task* child = TaskManager::instance().create_task(entry, argument, caller->address_space,
                                                          *caller->cpu);
        if (child == nullptr) {
            caller->context.x[10] = error_value(kErrorNoMemory);
            break;
        }

        if (!caller->cpu->scheduler->add_task(*child)) {
            hart::park();
        }

        caller->context.x[10] = child->tid;
        if (!g_create_reported) {
            printk("M00_08_TASK_CREATE: PASS\n");
            g_create_reported = true;
        }
        break;
    }

    case JIXIA_TASK_SYSCALL_END: {
        const TaskId ending_tid = caller->tid;
        const uintptr_t return_value = caller->context.x[10];
        const bool initial_task = ending_tid == TaskManager::instance().initial_task_id();
        const bool detached = caller->detached;

        if (!initial_task && return_value == JIXIA_M00_08_CHILD_ARGUMENT && !g_end_reported) {
            printk("M00_08_TASK_END: PASS\n");
            g_end_reported = true;
        }

        TaskManager::instance().end_task(*caller, return_value, ExitStatus::exited_clean);

        if (initial_task && detached && return_value == JIXIA_M00_08_INIT_RESULT) {
            printk("M00_08_TASK_LIFECYCLE: PASS\n");
        }
        break;
    }

    case JIXIA_TASK_SYSCALL_WAIT: {
        const int64_t tid = static_cast<int64_t>(caller->context.x[10]);
        const uintptr_t status_address = caller->context.x[11];
        const uintptr_t return_value_address = caller->context.x[12];

        if (status_address != 0U || return_value_address != 0U) {
            caller->context.x[10] = error_value(kErrorNotSupported);
            break;
        }

        const WaitResult result = TaskManager::instance().wait_task(*caller, tid);
        if (result == WaitResult::no_child) {
            caller->context.x[10] = error_value(kErrorDeadlock);
        } else if (result == WaitResult::completed && !g_wait_reported) {
            printk("M00_08_TASK_WAIT: PASS\n");
            g_wait_reported = true;
        } else if (result == WaitResult::blocked && !g_wait_block_reported) {
            printk("M00_08_TASK_WAIT_BLOCK: PASS\n");
            g_wait_block_reported = true;
        }
        break;
    }

    case JIXIA_TASK_SYSCALL_DETACH:
        caller->context.x[10] = TaskManager::instance().detach_task(*caller) ? 0U : 1U;
        if (caller->context.x[10] == 0U && !g_detach_reported) {
            printk("M00_08_TASK_DETACH: PASS\n");
            g_detach_reported = true;
        }
        break;

    default:
        return false;
    }

    TaskManager::restore_current_context(frame);
    return true;
}

} // namespace jixia::microkernel::task::syscall
