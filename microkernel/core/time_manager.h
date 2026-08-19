#pragma once

#include <stdint.h>

#include "microkernel/arch/riscv/hart_layout.h"
#include "microkernel/arch/riscv/trap_frame.h"
#include "microkernel/core/singleton.h"
#include "microkernel/core/spinlock.h"

namespace jixia::microkernel::hart {
struct HartLocal;
}

namespace jixia::microkernel::scheduler {
class Scheduler;
}

namespace jixia::microkernel::task {
struct Task;
}

namespace jixia::microkernel::time {

class DelayQueue final {
  public:
    DelayQueue();

    void reset();
    [[nodiscard]] bool insert(task::Task& task, uint64_t deadline);
    [[nodiscard]] task::Task* remove_expired(uint64_t now);
    [[nodiscard]] uint64_t ticks_until_next(uint64_t now, uint64_t maximum) const;

  private:
    mutable Spinlock lock_;
    task::Task* head_;
};

class TimeManager final {
  public:
    static TimeManager& instance();

    void initialize();
    [[nodiscard]] bool initialize_hart(hart::HartLocal& local);

    [[nodiscard]] uint64_t task_timeslice_ticks() const;
    [[nodiscard]] uint64_t idle_timeslice_ticks(const hart::HartLocal& local) const;

    [[nodiscard]] bool delay_task(task::Task& task, uint64_t seconds, uint64_t nanoseconds);
    void check_release_tasks(scheduler::Scheduler& scheduler);
    void handle_timer_interrupt(jixia::arch::riscv::TrapFrame& frame);
    void arm_current_timeslice() const;

  private:
    friend class jixia::microkernel::Singleton<TimeManager>;

    TimeManager();

    [[nodiscard]] static uint64_t duration_to_ticks(uint64_t seconds, uint64_t nanoseconds);

    DelayQueue delay_queues_[hart::kMaxHarts];
    uint64_t task_timeslice_ticks_;
    uint64_t idle_timeslice_ticks_;
    bool initialized_;
};

} // namespace jixia::microkernel::time
