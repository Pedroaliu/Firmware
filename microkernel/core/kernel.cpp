#include "microkernel/core/kernel.h"

#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/console/printk.h"
#include "microkernel/core/cpu_manager.h"
#include "microkernel/core/heap_manager.h"
#include "microkernel/core/scheduler.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/time_manager.h"
#include "microkernel/core/vmm_manager.h"
#include "microkernel/memory/page_manager.h"

extern "C" char jixia_user_init_task[];
extern "C" char jixia_user_preemption_init_task[];
extern "C" char jixia_user_ipc_init_task[];
extern "C" void jixia_release_executive_harts();
extern "C" [[noreturn]] void jixia_task_enter_first(jixia::arch::riscv::TrapFrame* frame,
                                                    uintptr_t satp);

namespace jixia::microkernel {

Kernel::Kernel() : cpp_ready_(false) {
}

Kernel& Kernel::instance() {
    return Singleton<Kernel>::instance();
}

void Kernel::cpp_bootstrap() {
    /*
     * Jixia currently has no .init_array dependency. Executive objects are
     * boot-hart-first function-local singletons. Keep this phase explicit so
     * a future constructor table fits the Hostboot bootstrap order.
     */
    cpp_ready_ = true;
}

bool Kernel::boot_data_bootstrap() {
    /*
     * Hostboot validates and records its bootloader handoff here. Jixia's
     * current QEMU contract is still only a0=hart ID and a1=DTB, so the phase
     * is deliberately present but has no persistent handoff object yet.
     */
    return true;
}

bool Kernel::memory_bootstrap() {
    memory::page_manager::reset();
    if (!memory::page_manager::add_contained_bootstrap_pool()) {
        return false;
    }

    memory::HeapManager::instance().initialize();
    return memory::VmmManager::instance().initialize();
}

bool Kernel::cpu_bootstrap(hart::HartIndex present_count) {
    time::TimeManager::instance().initialize();
    scheduler::Scheduler::instance().initialize();
    if (!task::TaskManager::instance().initialize()) {
        return false;
    }
    return cpu::CpuManager::instance().initialize(present_count);
}

void Kernel::platform_status_bootstrap() {
    /* Platform scratch/status publication is deferred until its ABI exists. */
}

void Kernel::debug_bootstrap() {
    /* The Hostboot-style debug pointer registry is not allocated pre-heap. */
}

bool Kernel::init_task_bootstrap() {
    hart::HartLocal& boot = cpu::CpuManager::instance().boot_hart();
    const auto& address_space = memory::VmmManager::instance().boot_address_space();

#if defined(JIXIA_M00_08_03_01_PROBE)
    const task::EntryPoint entry = reinterpret_cast<task::EntryPoint>(jixia_user_ipc_init_task);
#elif defined(JIXIA_M00_08_02_PROBE)
    const task::EntryPoint entry =
        reinterpret_cast<task::EntryPoint>(jixia_user_preemption_init_task);
#else
    const task::EntryPoint entry = reinterpret_cast<task::EntryPoint>(jixia_user_init_task);
#endif

    task::Task* initial =
        task::TaskManager::instance().create_task(entry, 0U, address_space, boot, true);
    if (initial == nullptr) {
        return false;
    }

    task::TaskManager::instance().set_initial_task(*initial);
    task::TaskManager::set_current_task(*initial);
    return true;
}

void Kernel::deferred_bootstrap() {
    /* DeferredQueue exists in the reference flow; Jixia has no work yet. */
}

[[noreturn]] void Kernel::dispatch_task() {
    task::Task* current = task::TaskManager::get_current_task();
    if (current == nullptr) {
        hart::park();
    }

    alignas(TRAP_FRAME_ALIGNMENT) jixia::arch::riscv::TrapFrame frame = {};
    task::TaskManager::restore_current_context(frame);
    memory::VmmManager::instance().prepare_first_user_dispatch();

    /*
     * Timer arming is executive mechanism, not probe workload: every M00-08
     * build arms the first absolute deadline before entering U mode. Probes
     * only select the acceptance workload and markers.
     */
    time::TimeManager::instance().arm_current_timeslice();

    if (hart::current().role == hart::HartRole::boot) {
        printk("M00_08_TASK_DISPATCH: PASS\n");
    }
    jixia_task_enter_first(&frame, current->context.satp);
}

[[noreturn]] void Kernel::secondary_bootstrap() {
    hart::HartLocal& local = hart::current();
    if (local.role != hart::HartRole::secondary || local.scheduler == nullptr ||
        local.idle_task == nullptr) {
        hart::park();
    }

    (void)local.scheduler->set_next_runnable();
    deferred_bootstrap();
    dispatch_task();
}

[[noreturn]] void Kernel::bootstrap(hart::HartIndex present_count) {
    /* Equivalent to Hostboot clearing SPRG3 before manager construction. */
    hart::current().current_task = nullptr;

    cpp_bootstrap();
    if (!cpp_ready_ || !boot_data_bootstrap() || !memory_bootstrap() ||
        !cpu_bootstrap(present_count)) {
        printk("M00_08_HOSTBOOT_BOOTSTRAP: FAIL\n");
        hart::park();
    }

    platform_status_bootstrap();
    debug_bootstrap();
    if (!init_task_bootstrap()) {
        printk("M00_08_HOSTBOOT_BOOTSTRAP: FAIL\n");
        hart::park();
    }

#if defined(JIXIA_M00_08_03_01_PROBE)
    const char* executive_header = "[Jixia][M00-08.03.01][IpcNonblocking]";
#elif defined(JIXIA_M00_08_02_PROBE)
    const char* executive_header = "[Jixia][M00-08.02][HostbootSchedulerAlignment]";
#else
    const char* executive_header = "[Jixia][M00-08.01][HostbootTaskFoundation]";
#endif
    /*
     * mtime preemption and deadline-aware idle are unconditional executive
     * mechanism in every M00-08 build; the probe only selects the acceptance
     * workload and markers. Both diagnostics describe the same scheduler.
     */
    const char* scheduler_mode = "global FIFO + per-hart affinity queue + mtime preemption";
    const char* time_mode = "per-hart sleep queue + deadline-aware idle slice";

    printk("\n"
           "%s\n"
           "cpp         : ready\n"
           "boot data   : QEMU handoff phase (dummy)\n"
           "page manager: contained bootstrap pool\n"
           "heap manager: fixed task storage (dynamic heap deferred)\n"
           "vmm         : shared bootstrap Sv39 root\n"
           "scheduler   : %s\n"
           "time        : %s\n"
           "task model  : tracker/wait/detach/idle\n"
           "platform    : status publication phase (dummy)\n"
           "debug       : pointer registry phase (dummy)\n"
           "M00_08_HOSTBOOT_BOOTSTRAP: PASS\n",
           executive_header, scheduler_mode, time_mode);

    /* Hostboot releases other hardware threads immediately before dispatch. */
    jixia_release_executive_harts();
    dispatch_task();
}

} // namespace jixia::microkernel
