#pragma once

#include <stddef.h>

#include "microkernel/arch/riscv/hart_layout.h"
#include "microkernel/core/singleton.h"
#include "microkernel/core/spinlock.h"

namespace jixia::microkernel::hart {
struct HartLocal;
}

namespace jixia::microkernel::task {
struct Task;
}

namespace jixia::microkernel::scheduler {

class RunQueue final {
  public:
    RunQueue();

    void reset();
    [[nodiscard]] bool insert(task::Task& task);
    [[nodiscard]] task::Task* remove();
    [[nodiscard]] size_t size() const;

  private:
    mutable Spinlock lock_;
    task::Task* head_;
    task::Task* tail_;
    size_t size_;
};

class Scheduler final {
  public:
    static Scheduler& instance();

    void initialize();
    void bind_hart(hart::HartLocal& local);

    [[nodiscard]] bool add_task(task::Task& task);
    void return_runnable();
    [[nodiscard]] task::Task& set_next_runnable();

  private:
    friend class jixia::microkernel::Singleton<Scheduler>;

    Scheduler();

    RunQueue global_run_queue_;
    RunQueue local_run_queues_[hart::kMaxHarts];
};

} // namespace jixia::microkernel::scheduler
