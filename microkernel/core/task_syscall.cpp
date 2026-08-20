#include "microkernel/core/task_syscall.h"

#include <stdint.h>

#include "microkernel/arch/riscv/task_syscall_abi.h"
#include "microkernel/arch/riscv/trap_cause.h"
#include "microkernel/arch/riscv/user_task_test_values.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/ipc_manager.h"
#include "microkernel/core/scheduler.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/time_manager.h"
#include "microkernel/core/vmm_manager.h"

namespace jixia::microkernel::task::syscall {
namespace {

constexpr uintptr_t kMstatusMppMask = uintptr_t{3U} << 11U;
constexpr intptr_t kErrorNoMemory = -12;
constexpr intptr_t kErrorInvalidArgument = -22;
constexpr intptr_t kErrorDeadlock = -35;
constexpr intptr_t kErrorNoSyscall = -38;
constexpr intptr_t kErrorNotSupported = -95;

bool g_create_reported = false;
bool g_context_switch_reported = false;
bool g_end_reported = false;
bool g_wait_reported = false;
bool g_wait_block_reported = false;
bool g_detach_reported = false;
bool g_idle_reported = false;

#ifdef JIXIA_M00_08_02_PROBE
bool g_sleep_reported = false;
bool g_sleep_resume_reported = false;
#endif

#ifdef JIXIA_M00_08_03_01_PROBE
bool g_ipc_create_reported = false;
bool g_ipc_enospc_reported = false;
bool g_ipc_destroy_reported = false;
bool g_ipc_nonowner_reported = false;
bool g_ipc_recv_attempted = false;
bool g_ipc_c01_sent_reported = false;
bool g_ipc_c01_got_reported = false;
bool g_ipc_c01_child_reported = false;
bool g_ipc_c03_got_1_reported = false;
bool g_ipc_c03_got_2_reported = false;
bool g_ipc_c03_got_3_reported = false;
bool g_ipc_c03_fifo_reported = false;
bool g_ipc_c14_malformed_reported = false;
bool g_ipc_c14_stale_reported = false;
bool g_ipc_c15_recycled_reported = false;
bool g_ipc_c15_isolated_reported = false;
bool g_ipc_c16_full_reported = false;
bool g_ipc_c16_oldest_reported = false;
bool g_ipc_c16_recovered_reported = false;
bool g_ipc_reserved_reported = false;
uint64_t g_ipc_last_destroyed_handle = 0U;
uint64_t g_ipc_full_handle = 0U;
#endif

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
    bool rescheduled = false;
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
        rescheduled = true;
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
        rescheduled = true;

        if (initial_task && detached && return_value == JIXIA_M00_08_INIT_RESULT) {
            printk("M00_08_TASK_LIFECYCLE: PASS\n");
        }

#ifdef JIXIA_M00_08_02_PROBE
        if (return_value == JIXIA_M00_08_SLEEP_RESULT && !g_sleep_resume_reported) {
            printk("M00_08_SLEEP_RESUME: PASS\n");
            g_sleep_resume_reported = true;
        }

        if (initial_task && detached && return_value == JIXIA_M00_08_PREEMPT_INIT_RESULT) {
            printk("M00_08_PREEMPTIVE_SCHEDULER: PASS\n");
            printk("M00_08_PREEMPTION_COUNT: %lu\n",
                   static_cast<unsigned long>(hart::current().scheduler_preemption_count));
        }
#endif

#ifdef JIXIA_M00_08_03_01_PROBE
        if ((return_value == JIXIA_M00_08_IPC_RECEIVER_CHILD_RESULT) && !g_ipc_c01_child_reported) {
            printk("M00_08_IPC_C01_SEND_BEFORE_RECV: PASS\n");
            g_ipc_c01_child_reported = true;
        }

        if ((return_value == JIXIA_M00_08_IPC_FIFO_CHILD_RESULT) && !g_ipc_c03_fifo_reported) {
            printk("M00_08_IPC_C03_FIFO: PASS\n");
            g_ipc_c03_fifo_reported = true;
        }

        if (initial_task && detached && return_value == JIXIA_M00_08_IPC_INIT_RESULT) {
            printk("M00_08_IPC_NONBLOCKING: PASS\n");
        }
#endif
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
        rescheduled = result == WaitResult::blocked;
        break;
    }

    case JIXIA_TASK_SYSCALL_DETACH:
        caller->context.x[10] = TaskManager::instance().detach_task(*caller) ? 0U : 1U;
        if (caller->context.x[10] == 0U && !g_detach_reported) {
            printk("M00_08_TASK_DETACH: PASS\n");
            g_detach_reported = true;
        }
        break;

    case JIXIA_TASK_SYSCALL_SLEEP: {
        const uint64_t seconds = caller->context.x[10];
        const uint64_t nanoseconds = caller->context.x[11];

        caller->context.x[10] = 0U;
        if (!time::TimeManager::instance().delay_task(*caller, seconds, nanoseconds)) {
            caller->context.x[10] = error_value(kErrorInvalidArgument);
            break;
        }

        (void)caller->cpu->scheduler->set_next_runnable();
        rescheduled = true;

#ifdef JIXIA_M00_08_02_PROBE
        if (!g_sleep_reported) {
            printk("M00_08_TASK_SLEEP: PASS\n");
            g_sleep_reported = true;
        }
#endif
        break;
    }

    case JIXIA_TASK_SYSCALL_ENDPOINT_CREATE: {
        uint64_t handle = 0U;
        const intptr_t result =
            ipc::EndpointManager::instance().create_endpoint(caller->tid, &handle);
        caller->context.x[10] = (result < 0) ? error_value(result) : handle;

#ifdef JIXIA_M00_08_03_01_PROBE
        if ((result == 0) && !g_ipc_create_reported) {
            printk("M00_08_IPC_ENDPOINT_CREATE: PASS\n");
            g_ipc_create_reported = true;
        }

        if ((result == 0) && (ipc::EndpointHandle::from_raw(handle).generation > 1U) &&
            !g_ipc_c15_recycled_reported) {
            printk("M00_08_IPC_C15_RECYCLED_GENERATION: PASS\n");
            g_ipc_c15_recycled_reported = true;
        }

        if ((result == ipc::kErrorNoSpace) && !g_ipc_enospc_reported) {
            printk("M00_08_IPC_ENDPOINT_ENOSPC: PASS\n");
            g_ipc_enospc_reported = true;
        }
#endif
        break;
    }

    case JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY: {
        const uint64_t handle = caller->context.x[10];
        const intptr_t result =
            ipc::EndpointManager::instance().destroy_endpoint(caller->tid, handle);
        caller->context.x[10] = (result < 0) ? error_value(result) : 0U;

#ifdef JIXIA_M00_08_03_01_PROBE
        if ((result == 0) && !g_ipc_destroy_reported) {
            g_ipc_last_destroyed_handle = handle;
            printk("M00_08_IPC_ENDPOINT_DESTROY: PASS\n");
            g_ipc_destroy_reported = true;
        }

        if ((result == ipc::kErrorAccess) && !g_ipc_nonowner_reported) {
            printk("M00_08_IPC_DESTROY_NONOWNER: PASS\n");
            g_ipc_nonowner_reported = true;
        }
#endif
        break;
    }

    case JIXIA_TASK_SYSCALL_SEND: {
        const uint64_t handle = caller->context.x[10];
        const uint64_t words[4] = {
            caller->context.x[11],
            caller->context.x[12],
            caller->context.x[13],
            caller->context.x[14],
        };
        const intptr_t result = ipc::EndpointManager::instance().send(caller->tid, handle, words);
        caller->context.x[10] = (result < 0) ? error_value(result) : 0U;

#ifdef JIXIA_M00_08_03_01_PROBE
        if (result == 0) {
            if ((words[0] == JIXIA_M00_08_IPC_WORD_C01) && !g_ipc_recv_attempted &&
                !g_ipc_c01_sent_reported) {
                printk("M00_08_IPC_C01_A_SENT: PASS\n");
                g_ipc_c01_sent_reported = true;
            }

            if ((g_ipc_full_handle != 0U) && (handle == g_ipc_full_handle) &&
                !g_ipc_c16_recovered_reported) {
                printk("M00_08_IPC_C16_RECOVER: PASS\n");
                g_ipc_c16_recovered_reported = true;
                g_ipc_full_handle = 0U;
            }
        } else if (result == ipc::kErrorAgain) {
            if (!g_ipc_c16_full_reported) {
                printk("M00_08_IPC_C16_FULL: PASS\n");
                g_ipc_c16_full_reported = true;
                g_ipc_full_handle = handle;
            }
        } else if (result == ipc::kErrorInvalidArgument) {
            if ((handle == JIXIA_M00_08_IPC_GARBAGE_HANDLE) && !g_ipc_c14_malformed_reported) {
                printk("M00_08_IPC_C14_MALFORMED: PASS\n");
                g_ipc_c14_malformed_reported = true;
            }

            if ((g_ipc_last_destroyed_handle != 0U) && (handle == g_ipc_last_destroyed_handle) &&
                !g_ipc_c14_stale_reported) {
                printk("M00_08_IPC_C14_STALE: PASS\n");
                g_ipc_c14_stale_reported = true;
            }
        }
#endif
        break;
    }

    case JIXIA_TASK_SYSCALL_TRY_RECV: {
        const uint64_t handle = caller->context.x[10];
        ipc::Message message{};
        const intptr_t result = ipc::EndpointManager::instance().try_recv(handle, &message);
        if (result < 0) {
            caller->context.x[10] = error_value(result);
        } else {
            caller->context.x[10] = 0U;
            caller->context.x[11] = message.words[0];
            caller->context.x[12] = message.words[1];
            caller->context.x[13] = message.words[2];
            caller->context.x[14] = message.words[3];
            caller->context.x[15] = message.sender;
        }

#ifdef JIXIA_M00_08_03_01_PROBE
        g_ipc_recv_attempted = true;

        if (result == 0) {
            const bool from_initial = message.sender == TaskManager::instance().initial_task_id();

            if ((message.words[0] == JIXIA_M00_08_IPC_WORD_C01) && from_initial &&
                !g_ipc_c01_got_reported) {
                printk("M00_08_IPC_C01_B_GOT: PASS\n");
                g_ipc_c01_got_reported = true;
            }

            if (from_initial && (message.words[0] == JIXIA_M00_08_IPC_WORD_C03_1) &&
                !g_ipc_c03_got_1_reported) {
                printk("M00_08_IPC_C03_GOT_1: PASS\n");
                g_ipc_c03_got_1_reported = true;
            }

            if (from_initial && (message.words[0] == JIXIA_M00_08_IPC_WORD_C03_2) &&
                !g_ipc_c03_got_2_reported) {
                printk("M00_08_IPC_C03_GOT_2: PASS\n");
                g_ipc_c03_got_2_reported = true;
            }

            if (from_initial && (message.words[0] == JIXIA_M00_08_IPC_WORD_C03_3) &&
                !g_ipc_c03_got_3_reported) {
                printk("M00_08_IPC_C03_GOT_3: PASS\n");
                g_ipc_c03_got_3_reported = true;
            }

            if ((message.words[0] == JIXIA_M00_08_IPC_WORD_C16_FIRST) &&
                !g_ipc_c16_oldest_reported) {
                printk("M00_08_IPC_C16_POP_OLDEST: PASS\n");
                g_ipc_c16_oldest_reported = true;
            }

            if ((message.words[0] == JIXIA_M00_08_IPC_WORD_C15) && !g_ipc_c15_isolated_reported) {
                printk("M00_08_IPC_C15_ISOLATION: PASS\n");
                g_ipc_c15_isolated_reported = true;
            }
        }
#endif
        break;
    }

    case JIXIA_TASK_SYSCALL_CALL:
    case JIXIA_TASK_SYSCALL_RECV:
    case JIXIA_TASK_SYSCALL_REPLY:
        /*
         * Numbers 9, 10, and 12 are frozen by the M00-08.03 ABI but reserved:
         * blocking call/reply IPC lands in later increments. Fail closed with
         * -ENOSYS instead of the crash path taken for undefined numbers.
         */
        caller->context.x[10] = error_value(kErrorNoSyscall);

#ifdef JIXIA_M00_08_03_01_PROBE
        if (!g_ipc_reserved_reported) {
            printk("M00_08_IPC_RESERVED_ENOSYS: PASS\n");
            g_ipc_reserved_reported = true;
        }
#endif
        break;

    default:
        printk("Invalid task syscall: %lu\n", static_cast<unsigned long>(number));
        TaskManager::instance().end_task(*caller, 0U, ExitStatus::crashed);
        rescheduled = true;
        break;
    }

    TaskManager::restore_current_context(frame);
    if (rescheduled) {
        time::TimeManager::instance().arm_current_timeslice();
    }
    return true;
}

} // namespace jixia::microkernel::task::syscall
