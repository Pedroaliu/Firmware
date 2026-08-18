#include "microkernel/core/time_manager.h"

#include "microkernel/core/hart.h"
#include "microkernel/core/singleton.h"

namespace jixia::microkernel::time {

TimeManager::TimeManager() : task_timeslice_ticks_(100000U), idle_timeslice_ticks_(1000000U) {
}

TimeManager& TimeManager::instance() {
    return Singleton<TimeManager>::instance();
}

void TimeManager::initialize() {
    /* Policy values are fixed until timer-driven dispatch is connected. */
}

void TimeManager::initialize_hart(hart::HartLocal& local) const {
    local.delay_list = nullptr;
    local.timeslice_ticks = task_timeslice_ticks_;
}

uint64_t TimeManager::task_timeslice_ticks() const {
    return task_timeslice_ticks_;
}

uint64_t TimeManager::idle_timeslice_ticks() const {
    return idle_timeslice_ticks_;
}

} // namespace jixia::microkernel::time
