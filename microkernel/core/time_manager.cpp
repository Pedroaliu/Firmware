#include "microkernel/core/time_manager.h"

#include <stdint.h>

#include "microkernel/console/printk.h"
#include "microkernel/core/hart.h"
#include "microkernel/core/scheduler.h"
#include "microkernel/core/singleton.h"
#include "microkernel/core/task.h"
#include "microkernel/core/task_manager.h"
#include "microkernel/core/timer.h"
#include "platform/qemu_virt/timer.h"

namespace jixia::microkernel::time {
namespace {

constexpr uint64_t kNanosecondsPerSecond = 1000000000ULL;

#ifdef JIXIA_M00_08_02_PROBE
bool g_sleep_wake_reported = false;
bool g_sleep_constants_reported = false;
uint64_t g_sleep_entry_time = 0U;
uint64_t g_sleep_duration_ticks = 0U;
#endif

[[noreturn]] void fail_closed() {
    hart::park();
}

} // namespace

DelayQueue::DelayQueue() : head_(nullptr) {
}

void DelayQueue::reset() {
    SpinlockGuard guard(lock_);
    head_ = nullptr;
}

bool DelayQueue::insert(task::Task& delayed_task, uint64_t deadline) {
    SpinlockGuard guard(lock_);

    if (delayed_task.delay.queued || delayed_task.queued ||
        delayed_task.state == task::TaskState::ended) {
        return false;
    }

    task::Task* previous = nullptr;
    task::Task* cursor = head_;
    while (cursor != nullptr && cursor->delay.deadline <= deadline) {
        previous = cursor;
        cursor = cursor->delay.next;
    }

    delayed_task.delay.previous = previous;
    delayed_task.delay.next = cursor;
    delayed_task.delay.deadline = deadline;
    delayed_task.delay.queued = true;

    if (previous != nullptr) {
        previous->delay.next = &delayed_task;
    } else {
        head_ = &delayed_task;
    }

    if (cursor != nullptr) {
        cursor->delay.previous = &delayed_task;
    }

    return true;
}

task::Task* DelayQueue::remove_expired(uint64_t now) {
    SpinlockGuard guard(lock_);

    task::Task* task = head_;
    if (task == nullptr || task->delay.deadline > now) {
        return nullptr;
    }

    head_ = task->delay.next;
    if (head_ != nullptr) {
        head_->delay.previous = nullptr;
    }

    task->delay.previous = nullptr;
    task->delay.next = nullptr;
    task->delay.queued = false;
    return task;
}

uint64_t DelayQueue::ticks_until_next(uint64_t now, uint64_t maximum) const {
    SpinlockGuard guard(lock_);

    if (head_ == nullptr) {
        return maximum;
    }

    if (head_->delay.deadline <= now) {
        return 1U;
    }

    const uint64_t remaining = head_->delay.deadline - now;
    return remaining < maximum ? remaining : maximum;
}

TimeManager::TimeManager()
    : delay_queues_{}, task_timeslice_ticks_(100000U), idle_timeslice_ticks_(1000000U),
      initialized_(false) {
}

TimeManager& TimeManager::instance() {
    return Singleton<TimeManager>::instance();
}

void TimeManager::initialize() {
    if (initialized_) {
        return;
    }

    for (DelayQueue& queue : delay_queues_) {
        queue.reset();
    }
    initialized_ = true;
}

bool TimeManager::initialize_hart(hart::HartLocal& local) {
    if (!initialized_ || local.index >= hart::kMaxHarts) {
        return false;
    }

    local.delay_list = &delay_queues_[local.index];
    local.timeslice_ticks = task_timeslice_ticks_;
    local.scheduler_preemption_count = 0U;
    return true;
}

uint64_t TimeManager::task_timeslice_ticks() const {
    return task_timeslice_ticks_;
}

uint64_t TimeManager::idle_timeslice_ticks(const hart::HartLocal& local) const {
    const auto* queue = static_cast<const DelayQueue*>(local.delay_list);
    if (queue == nullptr) {
        return idle_timeslice_ticks_;
    }

    const uint64_t now = jixia::platform::qemu_virt::timer::read_time();
    return queue->ticks_until_next(now, idle_timeslice_ticks_);
}

uint64_t TimeManager::duration_to_ticks(uint64_t seconds, uint64_t nanoseconds) {
    if (nanoseconds >= kNanosecondsPerSecond) {
        return 0U;
    }

    constexpr uint64_t kFrequency = jixia::platform::qemu_virt::timer::kFrequencyHz;
    if (seconds > UINT64_MAX / kFrequency) {
        return 0U;
    }

    const uint64_t seconds_ticks = seconds * kFrequency;
    const uint64_t nanoseconds_ticks =
        ((nanoseconds * kFrequency) + kNanosecondsPerSecond - 1U) / kNanosecondsPerSecond;
    if (seconds_ticks > UINT64_MAX - nanoseconds_ticks) {
        return 0U;
    }

    const uint64_t ticks = seconds_ticks + nanoseconds_ticks;
    return ticks == 0U ? 1U : ticks;
}

bool TimeManager::delay_task(task::Task& delayed_task, uint64_t seconds, uint64_t nanoseconds) {
    if (!initialized_ || task::TaskManager::get_current_task() != &delayed_task ||
        delayed_task.cpu == nullptr) {
        return false;
    }

    const uint64_t ticks = duration_to_ticks(seconds, nanoseconds);
    if (ticks == 0U) {
        return false;
    }

    auto* queue = static_cast<DelayQueue*>(delayed_task.cpu->delay_list);
    if (queue == nullptr) {
        return false;
    }

    const uint64_t now = jixia::platform::qemu_virt::timer::read_time();
    if (now > UINT64_MAX - ticks) {
        return false;
    }
    const uint64_t deadline = now + ticks;

    delayed_task.state = task::TaskState::blocked_sleep;
    delayed_task.state_info = reinterpret_cast<void*>(static_cast<uintptr_t>(deadline));
    if (!queue->insert(delayed_task, deadline)) {
        delayed_task.state = task::TaskState::running;
        delayed_task.state_info = nullptr;
        return false;
    }

#ifdef JIXIA_M00_08_02_PROBE
    if (!g_sleep_constants_reported) {
        /*
         * Diagnostic only: publish the timer-arming constants so the runner
         * derives its acceptance bound from the kernel's own slice values
         * instead of a magic number. Never affects scheduling behavior.
         */
        printk("M00_08_SCHED_SLICES: task=%lu idle=%lu\n",
               static_cast<unsigned long>(task_timeslice_ticks_),
               static_cast<unsigned long>(idle_timeslice_ticks_));
        g_sleep_constants_reported = true;
    }
    g_sleep_entry_time = now;
    g_sleep_duration_ticks = ticks;
#endif
    return true;
}

void TimeManager::check_release_tasks(scheduler::Scheduler& scheduler) {
    auto* queue = static_cast<DelayQueue*>(hart::current().delay_list);
    if (queue == nullptr) {
        fail_closed();
    }

    const uint64_t now = jixia::platform::qemu_virt::timer::read_time();
    while (task::Task* task = queue->remove_expired(now)) {
        task->state_info = nullptr;
        if (!scheduler.add_task(*task)) {
            fail_closed();
        }

#ifdef JIXIA_M00_08_02_PROBE
        if (!g_sleep_wake_reported) {
            printk("M00_08_SLEEP_WAKE: PASS\n");
            /*
             * Quantitative deadline-aware idle evidence: elapsed must land near
             * the requested duration, far below the idle-slice cap. The runner
             * asserts the bound; a fixed idle-slice timer would wake the sleeper
             * at or beyond that cap instead.
             */
            const uint64_t elapsed_ticks = now > g_sleep_entry_time ? now - g_sleep_entry_time : 0U;
            printk("M00_08_SLEEP_WAKE_EVIDENCE: elapsed=%lu requested=%lu\n",
                   static_cast<unsigned long>(elapsed_ticks),
                   static_cast<unsigned long>(g_sleep_duration_ticks));
            g_sleep_wake_reported = true;
        }
#endif
    }
}

void TimeManager::handle_timer_interrupt(jixia::arch::riscv::TrapFrame& frame) {
    task::Task* interrupted = task::TaskManager::get_current_task();
    if (interrupted == nullptr || interrupted->cpu == nullptr ||
        interrupted->cpu->scheduler == nullptr) {
        fail_closed();
    }

    task::TaskManager::save_current_context(frame);

    scheduler::Scheduler& scheduler = *interrupted->cpu->scheduler;
    check_release_tasks(scheduler);

    const task::TaskId interrupted_tid = interrupted->tid;
    const bool interrupted_idle = interrupted->idle;
    scheduler.return_runnable();
    task::Task& selected = scheduler.set_next_runnable();

    if (!interrupted_idle && selected.tid != interrupted_tid) {
        hart::HartLocal& local = hart::current();
        ++local.scheduler_preemption_count;

#ifdef JIXIA_M00_08_02_PROBE
        if (local.scheduler_preemption_count == 1U) {
            printk("M00_08_TIMER_PREEMPT: PASS\n");
        }
#endif
    }

    task::TaskManager::restore_current_context(frame);
    arm_current_timeslice();
}

void TimeManager::arm_current_timeslice() const {
    hart::HartLocal& local = hart::current();
    uint64_t ticks = local.timeslice_ticks;

    const uint64_t now = jixia::platform::qemu_virt::timer::read_time();
    const task::Task* current = task::TaskManager::get_current_task();
    if (current != nullptr && current->idle) {
        const auto* queue = static_cast<const DelayQueue*>(local.delay_list);
        if (queue == nullptr) {
            fail_closed();
        }
        ticks = queue->ticks_until_next(now, idle_timeslice_ticks_);
    }

    if (ticks == 0U) {
        ticks = 1U;
    }

    const uint64_t deadline = now > UINT64_MAX - ticks ? UINT64_MAX : now + ticks;

    /*
     * Program the absolute deadline computed above. Recomputing "now" in the
     * timer layer would let a busy idle-yield loop push a sleeper's deadline
     * forward a little on every syscall.
     *
     * This only enables MTIE. mstatus.MIE stays clear while the non-nestable
     * trusted trap stack is active; mret restores the selected U context.
     */
    timer::arm_task_deadline(deadline);
}

} // namespace jixia::microkernel::time
