#include "microkernel/core/cpu_manager.h"

#include "microkernel/core/scheduler.h"
#include "microkernel/core/singleton.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/time_manager.h"

namespace jixia::microkernel::cpu {

CpuManager::CpuManager() : present_count_(0U) {
}

CpuManager& CpuManager::instance() {
    return Singleton<CpuManager>::instance();
}

bool CpuManager::initialize(hart::HartIndex present_count) {
    if (present_count == 0U || present_count > hart::kMaxHarts) {
        return false;
    }

    hart::HartLocal* harts = hart::table();
    scheduler::Scheduler& scheduler = scheduler::Scheduler::instance();
    task::TaskManager& tasks = task::TaskManager::instance();
    time::TimeManager& time = time::TimeManager::instance();

    for (hart::HartIndex index = 0U; index < present_count; ++index) {
        hart::HartLocal& local = harts[index];
        scheduler.bind_hart(local);
        if (!time.initialize_hart(local)) {
            return false;
        }

        task::Task* idle = tasks.create_idle_task(local);
        if (idle == nullptr) {
            return false;
        }

        local.idle_task = idle;
        local.current_task = nullptr;
    }

    present_count_ = present_count;
    return true;
}

hart::HartLocal& CpuManager::current() const {
    return hart::current();
}

hart::HartLocal& CpuManager::boot_hart() const {
    return hart::table()[0];
}

hart::HartIndex CpuManager::present_count() const {
    return present_count_;
}

} // namespace jixia::microkernel::cpu
